%define _topdir	 	/root/mysql-udf-ipv6
%define name		mysql-udf-ipv6 
%define release		2	
%define version 	2.05
%define buildroot %{_topdir}/%{name}-%{version}-root

BuildRoot:	%{buildroot}
Summary: 		MySQL UDF IPv6
License: 		EUPL, v1.1
Name: 			%{name}
Version: 		%{version}
Release: 		%{release}
Source: 		tip.tar.gz
Prefix: 		/usr/lib/mysql
Group: 			devel
BuildRequires:		mysql-devel

%description
This library provides IPv6 inet_ntoa()/inet6_pton() and inet_aton()/inet6_ntop() support as user defined functions for MySQL.

%prep
%setup -q -n mysql-udf-ipv6

%build
make

%install
test -d /usr/lib/mysql/plugin || mkdir /usr/lib/mysql/plugin
#make install prefix=$RPM_BUILD_ROOT/usr
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/lib/mysql/plugin
make install 
cp /usr/lib/mysql/plugin/mysql_udf_ipv6.so $RPM_BUILD_ROOT/usr/lib/mysql/plugin/

%files
%defattr(-,root,root)
%doc README
%doc Changelog
/usr/lib/mysql/plugin/mysql_udf_ipv6.so

