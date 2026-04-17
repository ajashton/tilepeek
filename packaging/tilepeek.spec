Name:           tilepeek
Version:        0.8.0
Release:        1%{?dist}
Summary:        Preview and inspect MBTiles and PMTiles map tilesets
License:        MIT
URL:            https://github.com/ajashton/tilepeek
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.25
BuildRequires:  gcc-c++
BuildRequires:  qt6-qtbase-devel
BuildRequires:  qt6-qtsvg-devel
BuildRequires:  zlib-devel
BuildRequires:  librsvg2-tools
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       qt6-qtbase
Requires:       qt6-qtsvg
Requires:       zlib

%description
TilePeek is a lightweight desktop application for quickly previewing and
inspecting map tilesets stored in MBTiles and PMTiles container formats.
It supports raster and vector tiles with metadata and statistics display.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake
%cmake_build

%check
%ctest

%install
%cmake_install

desktop-file-validate %{buildroot}%{_datadir}/applications/com.tilepeek.TilePeek.desktop
appstream-util validate-relax --nonet %{buildroot}%{_datadir}/metainfo/com.tilepeek.TilePeek.metainfo.xml

%files
%license LICENSE
%doc README.md
%{_bindir}/tilepeek
%{_datadir}/applications/com.tilepeek.TilePeek.desktop
%{_datadir}/metainfo/com.tilepeek.TilePeek.metainfo.xml
%{_datadir}/mime/packages/com.tilepeek.TilePeek.mime.xml
%{_datadir}/icons/hicolor/scalable/apps/com.tilepeek.TilePeek.svg
%{_datadir}/icons/hicolor/*/apps/com.tilepeek.TilePeek.png
