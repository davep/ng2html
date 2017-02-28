all: ng2html ng2html.man

ng2html : ng2html.c cfgfile.c
	gcc -Wall -O2 -s ng2html.c cfgfile.c -o ng2html

ng2html.man: ng2html.1
	groff -man -T ascii ng2html.1 > ng2html.man

clean:
	-rm -f core *.o *~ ng2html
