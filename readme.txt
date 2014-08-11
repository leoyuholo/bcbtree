To compile bcbtree_range_count under Linux:

1. get gtk+
	
	sudo apt-get install libgtk2.0-dev
	
2. go to corresponding directory and make

	make clean;make
	
3. start and have fun!

To compile bcbtree_range_count under Windows:

1. download MinGW

	go to http://www.mingw.org/ and download MinGW
	
2. download GTK all-in-one bundle
	
	go to http://ftp.gnome.org/pub/gnome/binaries/win32/gtk+/2.12/gtk+-bundle-2.12.11.zip and download
	
3. install MinGW
	
	install with base tools, g++ and make
	
4. extract GTK all-in-one bundle

5. add the corrsponding path to environment variables
	
	add the path of bin folder of MinGW and bin folder of extracted GTK all-in-one bundle to system environment variables. Under Windows 7, right-click "My Computer" -> "Properties" -> "Advanced System Settings" -> "Environment Variables" -> "System variables" -> "Path" -> "Edit". Remember to separate two path with a semicolon ";".
	
6. place source code under MinGW\msys\1.0\

7. run msys.bat in MinGW\msys\1.0\

8. go to the directory of source code, root directory "/" of msys is MinGW\msys\1.0\

9. use command "make" to compile

10. if an error message of not declared "int64_t" or "u_int64_t"
	
	add the following two lines to gammaEliasCode.h
	
	#define int64_t	long long int
	#define u_int64_t	unsigned long long int
	
11. if an error message of no reference to "g_thread_init()"
	
	add the flag "-lgthread-2.0" to makefile under the rule "range count:" in front of "`pkg-config --cflags --libs gtk+-2.0`"
	
12. start and have fun!
