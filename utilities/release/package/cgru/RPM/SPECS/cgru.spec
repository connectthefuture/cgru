# This is RPM spec file:

%define _topdir %(echo $PWD)/RPM

Summary:       CGRU
License:       GPL
Name:          cgru
Version:       @VERSION@
Release:       @RELEASE@
Group:         Applications/Graphics

Requires:      cgru-common = @VERSION@, afanasy-doc = @VERSION@, afanasy-gui = @VERSION@, afanasy-render = @VERSION@, afanasy-plugins = @VERSION@, afanasy-examples = @VERSION@

%description
Description of myrpmtest.

%prep

%build

%install
cd ../..
dirs="usr opt"
for dir in $dirs; do
   mkdir -p $RPM_BUILD_ROOT/$dir
   mv $dir/* $RPM_BUILD_ROOT/$dir
done

%files
%defattr(-,root,root)
/opt
/usr

%clean
