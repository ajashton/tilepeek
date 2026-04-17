# Packaging TilePeek

This directory holds scripts and specs for building distributable packages.
Each script is self-contained and reads the version from the top-level
`CMakeLists.txt`.

| Platform | Output       | Script            |
| -------- | ------------ | ----------------- |
| macOS    | `.dmg`       | `build-macos.sh`  |
| Linux    | `.rpm`       | `build-rpm.sh`    |
| Windows  | _TBD_        | _not yet_         |

---

## macOS (`.dmg`)

`build-macos.sh` produces a signed, notarized, Apple Silicon (`arm64`)
`TilePeek-<version>.dmg` at `build/macos/`. The script must run on an Apple
Silicon Mac; Intel Mac support is not provided.

### Apple Developer setup for signing

Building the a distributable MacOS app requires enrolling in the Apple
Developer program and setting up certificates for signing and notarizing.

<details>
<summary>Signing setup details</summary>

#### 1. Enroll in the Apple Developer Program

Go to <https://developer.apple.com/programs/> and enroll. You need a $99/yr
individual or organization membership; the free Apple ID tier cannot issue
Developer ID certificates.

#### 2. Create a _Developer ID Application_ certificate

This is the certificate used to sign apps distributed outside the Mac App
Store. It is **not** the same as "Apple Development" (for local builds) or
"Developer ID Installer" (for `.pkg` files).

1. Open **Keychain Access** → menu **Certificate Assistant** → **Request a
   Certificate From a Certificate Authority**. Fill in your email, leave
   "CA Email" blank, select **Saved to disk**, and save the `.certSigningRequest`
   file.
2. Go to <https://developer.apple.com/account/resources/certificates/list>.
   Click the **+** button.
3. Under **Software**, select **Developer ID Application** → Continue.
4. Upload the CSR file from step 1 → Continue → Download. This gives you
   a `.cer` file; double-click it to install it into the login keychain.
5. Keychain Access will now list a certificate named
   `Developer ID Application: Your Name (TEAMID)`, with an expandable private
   key underneath it.

Verify the install and note the exact identity name:

```sh
security find-identity -v -p codesigning
# Look for a line like:
#   1) ABCDEF1234...  "Developer ID Application: Your Name (ABCDE12345)"
```

Copy the full quoted string — that is your `MACOS_SIGNING_IDENTITY`.

> [!NOTE]
> **Back up the private key.** If you lose it, you cannot sign updates with the
> same identity. Export the certificate + private key from Keychain Access as a
> password-protected `.p12` file and store it somewhere safe (1Password, a
> USB drive in a safe, etc.). **Do not commit it to the repo.**

#### 3. Create an app-specific password for notarization

1. Go to <https://account.apple.com/>, sign in with the Apple ID attached to
   your developer account.
2. Under **Sign-In and Security** → **App-Specific Passwords**, click **+** and
   label it `tilepeek-notary`.
3. Copy the generated password (format `xxxx-xxxx-xxxx-xxxx`). You only see
   it once; you can always revoke and regenerate.

Store it in the login keychain via `notarytool` (one-time):

```sh
xcrun notarytool store-credentials "tilepeek-notary" \
    --apple-id "you@example.com" \
    --team-id "TEAMID" \
    --password "xxxx-xxxx-xxxx-xxxx"
```

The `TEAMID` is the 10-character code inside the parentheses of your signing
identity, or visible at <https://developer.apple.com/account> → Membership.

After this, the app-specific password is kept in your keychain only — no
scripts, env vars, or files in this repo need to know it. Verify:

```sh
xcrun notarytool history --keychain-profile "tilepeek-notary"
```

</details>

### Install build dependencies

```sh
brew install cmake qt@6 create-dmg librsvg
```

- `cmake` — build system
- `qt@6` — Qt 6 framework. Homebrew's arm64-native build is exactly what we
  want for an arm64-only release
- `create-dmg` — layout tool for the `.dmg`
- `librsvg` — provides `rsvg-convert`, used to render the app icon at multiple
  sizes before `iconutil` packs them into `.icns`

`iconutil`, `codesign`, `xcrun`, and `security` ship with the Xcode Command
Line Tools; install those if you haven't already:

```sh
xcode-select --install
```

### Per-release build

Once setup is done, a release is two exports and a script:

```sh
export MACOS_SIGNING_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export MACOS_NOTARY_PROFILE="tilepeek-notary"

./packaging/build-macos.sh
```

If you installed Qt somewhere other than Homebrew, also `export QT_DIR=…`
pointing at the Qt 6 prefix. Otherwise the script resolves it via
`brew --prefix qt@6`.

Output: `build/macos/TilePeek-<version>.dmg` plus its SHA-256.

Useful flags during development:

- `--skip-tests` — don't run `ctest` after the build
- `--skip-notarize` — build and locally sign, but skip the Apple round-trip.
  The resulting DMG will trigger a Gatekeeper warning if given to someone else.
  Fine for local smoke testing.
- `--keep-build` — don't wipe `build/macos` before configuring. Saves time on
  iterative runs.

### What the script does, in order

1. Validates prereqs: host is Apple Silicon, tools on `$PATH`, signing
   identity is in the keychain, notary profile works.
2. `cmake -S . -B build/macos` with `CMAKE_OSX_ARCHITECTURES=arm64`,
   `CMAKE_OSX_DEPLOYMENT_TARGET=14.0`, `CMAKE_PREFIX_PATH=$QT_DIR`.
3. `cmake --build` (Release).
4. `ctest` unless `--skip-tests`.
5. Copies the built `TilePeek.app` to `build/macos/stage/`.
6. `macdeployqt` bundles Qt frameworks and plugins into the `.app`.
7. Recursively signs every dylib, framework, and executable in the bundle with
   hardened runtime and a secure timestamp, then signs the outer `.app` with
   `resources/macos/tilepeek.entitlements`.
8. `create-dmg` builds the DMG with a branded layout (drag-to-Applications).
9. Signs the DMG itself.
10. `xcrun notarytool submit --wait` ships the DMG to Apple; waits for
    approval.
11. `xcrun stapler staple` embeds the notarization ticket into the DMG so it
    works offline.
12. `spctl -a -vv -t install` confirms Gatekeeper accepts it.

### Sanity checks on the built DMG

```sh
# The DMG is stapled (offline verification works).
xcrun stapler validate build/macos/TilePeek-*.dmg

# Gatekeeper accepts it.
spctl -a -vv -t install build/macos/TilePeek-*.dmg
```

### Troubleshooting

- **"errSecInternalComponent" during codesign.** The signing identity's
  private key isn't unlocked. Run `security unlock-keychain ~/Library/Keychains/login.keychain-db`
  and try again, or open Keychain Access and unlock the login keychain.
- **Notarization rejected with "The binary is not signed with a valid
  Developer ID certificate."** Check that the identity name is _Developer ID
  Application_, not _Apple Development_, and that you don't have an expired
  copy in the keychain shadowing it.
- **Notarization rejected with "The signature does not include a secure
  timestamp."** The script always passes `--timestamp`; the usual cause is
  running the codesign step while offline. Reconnect and re-run.
- **Gatekeeper still warns after notarization.** You forgot to staple. Run
  `xcrun stapler staple path/to/TilePeek-x.y.z.dmg`, or just re-run the script.

### What's in the repo vs. what stays local

| In the repo                             | Local only                              |
| --------------------------------------- | --------------------------------------- |
| `packaging/build-macos.sh`              | Developer ID certificate + private key  |
| `resources/macos/Info.plist.in`         | App-specific password for notarization  |
| `resources/macos/tilepeek.entitlements` | Keychain profile named `tilepeek-notary`|
| This README                             | `$MACOS_SIGNING_IDENTITY` env var       |
|                                         | `$MACOS_NOTARY_PROFILE` env var         |

The signing identity _name_ and team ID are embedded in every signed binary
you publish — they are not secret. The _private key_ in your keychain and the
_app-specific password_ are secrets; keep them out of the repo, CI logs, issue
comments, and screenshots.

---

## Linux (`.rpm`)

`build-rpm.sh` builds a binary and source RPM for Fedora / RHEL-like
distributions. It pairs with `tilepeek.spec`.

### Prerequisites

On a Fedora / RHEL host (or in a container):

```sh
sudo dnf install rpm-build rpmdevtools \
    cmake gcc-c++ qt6-qtbase-devel zlib-devel \
    librsvg2-tools desktop-file-utils libappstream-glib
```

The `BuildRequires` line in `tilepeek.spec` is the authoritative list.

### Build

```sh
./packaging/build-rpm.sh
```

Outputs under `build/rpmbuild/RPMS/<arch>/` and `build/rpmbuild/SRPMS/`.

If the working tree is clean the script uses `git archive` to produce the
source tarball. If it's dirty it falls back to `tar` on the working tree and
warns — fine for iterating locally, but always commit before cutting a real
release so the SRPM is reproducible.

### DEB packaging

Not yet implemented. Contributions welcome; `tilepeek.spec` is a reasonable
starting reference for the build & install steps.

---

## Windows

Not yet implemented. The `main.cpp` `QFileOpenEvent` handler is a no-op on
Windows (file associations pass paths via argv, which is already handled). A
contributor comfortable with MSVC + `windeployqt` + WiX or NSIS would be very
welcome.
