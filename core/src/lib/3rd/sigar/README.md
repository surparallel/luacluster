# SIGAR - System Information Gatherer And Reporter

Library for getting information about hardware and operating system resources.

Base library is written in C with bindings to Java, C# and Go

This repository contains independent fork of official "hyperic/sigar" repository maintained by VMware.

Current documentation and binaries for version 1.6.4 are at SourceForge: https://sourceforge.net/projects/sigar/

#### Target support for 2.0.0:
* Architectures:
    * amd64 (x86-64)
    * ARMv7
    * ARMv8
* Operating systems:
    * Windows
        * Windows 7
        * Windows 8
        * Windows 8.1
        * Windows 10
    * Windows Server
        * Windows Server 2008 R2
        * Windows Server 2012
        * Windows Server 2012 R2
        * Windows Server 2016
    * OS X
        * OS X 10.8
        * OS X 10.9
        * OS X 10.10
        * OS X 10.11
        * macOS 10.12
    * Linux
        * Kernel: LTS releases, 3.2+
        * Distributions:
            * Debian: 7+
            * Ubuntu: LTS releases: 14.04+
            * Fedora: 24+
            * CentOS: 6+
            * openSuse: 42.1+

* Bindings:
    * C++: std >= C++11
    * Java: 7+
    * C#: 5+ (.NET Framework 4.5.2+ and .NET Core)
    * Go: 1.7+
    * Swift: 3
