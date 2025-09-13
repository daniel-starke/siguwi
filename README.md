siguwi
======

**Si**gning **GU**I for **Wi**ndows.  
This project provides a simple GUI application for running code signing
applications on Windows. The certificates and keys are expected to be located
on SmartCard or USB token.

Features
========

- supports [osslcodesign][1] and similar tools
- only a single pin entry for subsequent calls needed per configuration
- GUI for progress and final state
- pre-configurable setups
- command-line controlled

[1]: https://github.com/mtrojnar/osslsigncode

Usage
=====

This application requires Windows 7 or newer.

Step 1
------

Run `siguwi.exe`. Choose a signing certificate and adjust the `signApp`
command-line. Store the configuration to an INI file in the path of `siguwi.exe`.
For example `config.ini`.

Note that the given command-line runs from within the `siguwi.exe` directory.

Step 2
------

Run `siguwi.exe -c config.ini unsigned-app.exe`.  
Or integrate `siguwi.exe -c config.in %1` into the shell context menu by configuring
the Windows registry. Note that `siguwi.ini` is being used if no `-c` option was given.

Shell Integration
=================

After creating the INI configuration file in Step 1 it is possible to integrate
the application into the Windows shell context menu by running the following
command.

```bat
siguwi.exe -c config.ini -r "siguwi:Sign with siguwi"
```

Different configurations can be added by using different verbs. E.g. `siguwi2`.

The integration can be undone by the following command.

```bat
siguwi.exe -c config.ini -u siguwi
```

The changes may do not take effect until logged and in once. Or run the following command.

```bat
TASKKILL /F /IM explorer.exe & START explorer.exe
```

Run the commands as administrator to install the shell context menu entry for all users.

Building
========

The following dependencies are given.  
- C17
- MinGW-W64
  
```sh
make
```

This creates the target application:
- `bin\siguwi`

Files
=====

|Name                |Meaning
|--------------------|--------------------------------------------------------------
|common.mk           |Generic Makefile setup.
|argp*, getopt*      |Command-line parser.
|htableo.*           |Object based hash tables.
|rcwstr.*            |Reference counted wide-character strings.
|resource.*          |Executable resource data.
|siguwi.exe.manifest |Executable manifest.
|siguwi.h            |Main application header file.
|siguwi-config.c     |Configuration window utility functions.
|siguwi-ini.c        |INI configuration utility functions
|siguwi-main.c       |Main application 
|siguwi-process.c    |Process window utility functions.
|siguwi-registry.c   |Shell context menu integration via registry utility functions.
|siguwi-translate.c  |Character encoding translation utility functions.
|strbuf.i            |Generic string buffers.
|target.h            |Target specific functions and macros.
|ustrbuf.*           |Wide-character string buffers.
|utf8.*              |UTF-8 support functions.
|vector.*            |Object based dynamic arrays.

License
=======

See [LICENSE](LICENSE).  

Contributions
=============

No content contributions are accepted. Please file a bug report or feature request instead.  
This decision was made in consideration of the used license.
