Name:           vss
Version:        1.0.0
Release:        1%{?dist}
Summary:        The VSS Programming Language Compiler and VM

License:        MIT
URL:            https://github.com/siddharth-1118/vss-language
Source0:        https://github.com/siddharth-1118/vss-language/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  make

%description
VSS is a production-ready, high-performance programming language featuring
automatic reference counting and a custom stack-based virtual machine.

%prep
%setup -q

%build
cd vss
make clean
make

%install
mkdir -p %{buildroot}%{_bindir}
install -m 0755 vss/vss %{buildroot}%{_bindir}/vss

%files
%{_bindir}/vss

%changelog
* Mon Jun 29 2026 VSS Core Team <maintainers@vss-language.org> - 1.0.0-1
- Initial release of VSS VM Compiler and CLI.
