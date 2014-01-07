%define version    	 8.0
Name:		ck
Version:	%{version}
Release:	14
Group:		Programming/Interpreter
Summary:	Tk-like Curses toolkit on top of Tcl
License:	BSD
Packager:	<chw@ch-werner.de>
URL:		http://www.ch-werner.de/ck
BuildRoot:	/var/tmp/%{name}%{version}
Source:		http://www.ch-werner.de/ck/ck%{version}.tar.gz
Requires:	tcl >= %{version}

%description
Ck is a (XPG4|n)curses widget set modelled after Tk designed to work
closely with the tcl scripting language. It allows you to write simple
programs with full featured console mode UIs. Tcl/Ck applications can
also be run on Windows platforms in console mode.

%prep
%setup -n %{name}%{version}

%build
./configure --prefix=/usr --disable-shared --enable-gcc --with-tcl=/usr/lib
ckversion=`. ckConfig.sh ; echo $CK_VERSION`
make CFLAGS="$RPM_OPT_FLAGS" libck${ckversion}.a
mv libck${ckversion}.a /tmp
make distclean
mv /tmp/libck${ckversion}.a .
./configure --prefix=/usr --enable-shared --enable-gcc --with-tcl=/usr/lib
make CFLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/usr/lib
mkdir -p $RPM_BUILD_ROOT/usr/man/man{1,3,n}
make INSTALL_ROOT=$RPM_BUILD_ROOT install install-man
cp -p ckConfig.sh $RPM_BUILD_ROOT/usr/lib
ckversion=`. ckConfig.sh ; echo $CK_VERSION`
ln -sf libck${ckversion}.so $RPM_BUILD_ROOT/usr/lib/libck.so
cp -p libck${ckversion}.a $RPM_BUILD_ROOT/usr/lib
ln -sf libck${ckversion}.a $RPM_BUILD_ROOT/usr/lib/libck.a
mv $RPM_BUILD_ROOT/usr/bin/cwsh $RPM_BUILD_ROOT/usr/bin/cwsh${ckversion}
ln -sf cwsh${ckversion} $RPM_BUILD_ROOT/usr/bin/cwsh
rm -rf $RPM_BUILD_ROOT/usr/include
rm -rf $RPM_BUILD_ROOT/usr/man/man3
mkdir -p $RPM_BUILD_ROOT/usr/share/ck-${ckversion}/man
mv $RPM_BUILD_ROOT/usr/man/mann $RPM_BUILD_ROOT/usr/share/ck-${ckversion}/man
find $RPM_BUILD_ROOT/usr/share/ck-${ckversion}/man -type f -exec gzip {} \;
find $RPM_BUILD_ROOT/usr/man -type f -exec gzip {} \;
if test "%{_mandir}" != "/usr/man" ; then
    mkdir $RPM_BUILD_ROOT%{_mandir}
    mv $RPM_BUILD_ROOT/usr/man/man1 $RPM_BUILD_ROOT%{_mandir}
fi

%clean
rm -rf $RPM_BUILD_ROOT

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root)
/usr/bin/*
/usr/lib/lib*
/usr/lib/ck*
%{_mandir}/man1/*
/usr/share/ck-*

%changelog
* Mon Jun 14 2004 <chw@ch-werner.de>
- eliminated more compiler warnings

* Thu Jun 10 2004 <chw@ch-werner.de>
- more fixes concerning UTF-8 locales

* Wed Jun 09 2004 <chw@ch-werner.de>
- fixes for newer RPM versions

* Tue Jan 28 2003 <chw@ch-werner.de>
- fixes for Tcl 8.4.1

* Sun Jul 14 2002 <chw@ch-werner.de>
- added support for Tcl 8.4 (thanks to <michael@cleverly.com>
- added event source code for Tcl >= 8.0

* Sun Apr 21 2002 <chw@ch-werner.de>
- added fix from <dfs@roaringpenguin.com> in ckMessage.c

* Wed Sep 05 2001 <chw@ch-werner.de>
- fixes in ckUtil.c, ckTextDisp.c, ckMessage.c most related
  to CkMeasureChars cleanup.

* Sun Aug 26 2001 <chw@ch-werner.de>
- environment variables CK_USE_ENCODING and CK_USE_GPM for
  controlling standard encoding (Tcl >= 8.1) and GPM usage,
  various fixes for UTF-8 handling and Win32 code pages.

* Tue May 15 2001 <Christian.Werner@t-online.de>
- fixed initial screen flashing, added -noclear option in exit cmd

* Thu Dec 07 2000 <Christian.Werner@t-online.de>
- fixes for Tcl versions >= 8.1 (UTF8 handling)

* Fri Nov 24 2000 <Christian.Werner@t-online.de>
- fixed Tcl version handling in configure

* Wed Sep 20 2000 <Christian.Werner@t-online.de>
- rebuilt with ckEvent fixes

* Sun Aug 27 2000 <Christian.Werner@t-online.de>
- repackaged with new Ck distrib

* Fri Aug 25 2000 <Christian.Werner@t-online.de>
- created

