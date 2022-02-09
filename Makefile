.PHONY: regress
.SUFFIXES: .xml .md .html .pdf .1 .1.html .3 .3.html .5 .5.html .thumb.jpg .png .in.pc .pc

include Makefile.configure

VERSION		 = 0.10.0
OBJS		 = autolink.o \
		   buffer.o \
		   diff.o \
		   document.o \
		   entity.o \
		   gemini.o \
		   html.o \
		   html_escape.o \
		   xelatex.o \
		   latex.o \
		   library.o \
		   libdiff.o \
		   nroff.o \
		   odt.o \
		   smartypants.o \
		   term.o \
		   tree.o \
		   util.o
COMPAT_OBJS	 = compats.o
WWWDIR		 = /var/www/vhosts/kristaps.bsd.lv/htdocs/mdown
HTMLS		 = archive.html \
		   atom.xml \
		   diff.html \
		   diff.diff.html \
		   index.html \
		   README.html \
		   $(MANS)
MANS		 = man/mdown.1.html \
		   man/mdown.3.html \
		   man/mdown.5.html \
		   man/mdown-diff.1.html \
		   man/mdown_buf.3.html \
		   man/mdown_buf_diff.3.html \
		   man/mdown_buf_free.3.html \
		   man/mdown_buf_new.3.html \
		   man/mdown_diff.3.html \
		   man/mdown_doc_free.3.html \
		   man/mdown_doc_new.3.html \
		   man/mdown_doc_parse.3.html \
		   man/mdown_file.3.html \
		   man/mdown_file_diff.3.html \
		   man/mdown_gemini_free.3.html \
		   man/mdown_gemini_new.3.html \
		   man/mdown_gemini_rndr.3.html \
		   man/mdown_html_free.3.html \
		   man/mdown_html_new.3.html \
		   man/mdown_html_rndr.3.html \
		   man/mdown_latex_free.3.html \
		   man/mdown_latex_new.3.html \
		   man/mdown_latex_rndr.3.html \
		   man/mdown_metaq_free.3.html \
		   man/mdown_node_free.3.html \
		   man/mdown_nroff_free.3.html \
		   man/mdown_nroff_new.3.html \
		   man/mdown_nroff_rndr.3.html \
		   man/mdown_odt_free.3.html \
		   man/mdown_odt_new.3.html \
		   man/mdown_odt_rndr.3.html \
		   man/mdown_term_free.3.html \
		   man/mdown_term_new.3.html \
		   man/mdown_term_rndr.3.html \
		   man/mdown_tree_rndr.3.html
SOURCES		 = autolink.c \
		   buffer.c \
		   compats.c \
		   diff.c \
		   document.c \
		   entity.c \
		   gemini.c \
		   html.c \
		   html_escape.c \
		   xelatex.c \
		   latex.c \
		   libdiff.c \
		   library.c \
		   main.c \
		   nroff.c \
		   odt.c \
		   smartypants.c \
		   term.c \
		   tests.c \
		   tree.c \
		   util.c
HEADERS 	 = extern.h \
		   libdiff.h \
		   mdown.h \
		   term.h
PDFS		 = diff.pdf \
		   diff.diff.pdf \
		   index.latex.pdf \
		   index.mandoc.pdf \
		   index.nroff.pdf
MDS		 = index.md README.md
CSSS		 = diff.css template.css
JSS		 = diff.js
IMAGES		 = screen-mandoc.png \
		   screen-groff.png \
		   screen-term.png
THUMBS		 = screen-mandoc.thumb.jpg \
		   screen-groff.thumb.jpg \
		   screen-term.thumb.jpg

# Only for MarkdownTestv1.0.3.

REGRESS_ARGS	 = "--out-no-smarty"
REGRESS_ARGS	+= "--parse-no-img-ext"
REGRESS_ARGS	+= "--parse-no-metadata"
REGRESS_ARGS	+= "--html-no-head-ids"
REGRESS_ARGS	+= "--html-no-skiphtml"
REGRESS_ARGS	+= "--html-no-escapehtml"
REGRESS_ARGS	+= "--html-no-owasp"
REGRESS_ARGS	+= "--html-no-num-ent"
REGRESS_ARGS	+= "--parse-no-autolink"
REGRESS_ARGS	+= "--parse-no-cmark"
REGRESS_ARGS	+= "--parse-no-deflists"

all: mdown mdown-diff mdown.pc

www: $(HTMLS) $(PDFS) $(THUMBS) mdown.tar.gz mdown.tar.gz.sha512

installwww: www
	mkdir -p $(WWWDIR)/snapshots
	$(INSTALL) -m 0444 $(THUMBS) $(IMAGES) $(MDS) $(HTMLS) $(CSSS) $(JSS) $(PDFS) $(WWWDIR)
	$(INSTALL) -m 0444 mdown.tar.gz $(WWWDIR)/snapshots/mdown-$(VERSION).tar.gz
	$(INSTALL) -m 0444 mdown.tar.gz.sha512 $(WWWDIR)/snapshots/mdown-$(VERSION).tar.gz.sha512
	$(INSTALL) -m 0444 mdown.tar.gz $(WWWDIR)/snapshots
	$(INSTALL) -m 0444 mdown.tar.gz.sha512 $(WWWDIR)/snapshots

mdown: libmdown.a main.o
	$(CC) -o $@ main.o libmdown.a $(LDFLAGS) $(LDADD_MD5) -lm

mdown-diff: mdown
	ln -f mdown mdown-diff

libmdown.a: $(OBJS) $(COMPAT_OBJS)
	$(AR) rs $@ $(OBJS) $(COMPAT_OBJS)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(LIBDIR)/pkgconfig
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	mkdir -p $(DESTDIR)$(MANDIR)/man3
	mkdir -p $(DESTDIR)$(MANDIR)/man5
	mkdir -p /usr/share/fonts/Persian/
	cp -rfv font-farsi/Kayhan /usr/share/fonts/Persian/
	$(INSTALL_DATA) mdown.pc $(DESTDIR)$(LIBDIR)/pkgconfig
	$(INSTALL_PROGRAM) mdown $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) mdown-diff $(DESTDIR)$(BINDIR)
	$(INSTALL_LIB) libmdown.a $(DESTDIR)$(LIBDIR)
	$(INSTALL_DATA) mdown.h $(DESTDIR)$(INCLUDEDIR)
	for f in $(MANS) ; do \
		name=`basename $$f .html` ; \
		section=$${name##*.} ; \
		$(INSTALL_MAN) man/$$name $(DESTDIR)$(MANDIR)/man$$section ; \
	done

distcheck: mdown.tar.gz.sha512
	mandoc -Tlint -Werror man/*.[135]
	newest=`grep "<h1>" versions.xml | tail -1 | sed 's![ 	]*!!g'` ; \
	       [ "$$newest" = "<h1>$(VERSION)</h1>" ] || \
		{ echo "Version $(VERSION) not newest in versions.xml" 1>&2 ; exit 1 ; }
	[ "`openssl dgst -sha512 -hex mdown.tar.gz`" = "`cat mdown.tar.gz.sha512`" ] || \
 		{ echo "Checksum does not match." 1>&2 ; exit 1 ; }
	rm -rf .distcheck
	mkdir -p .distcheck
	( cd .distcheck && tar -zvxpf ../mdown.tar.gz )
	( cd .distcheck/mdown-$(VERSION) && ./configure PREFIX=prefix )
	( cd .distcheck/mdown-$(VERSION) && $(MAKE) )
	( cd .distcheck/mdown-$(VERSION) && $(MAKE) regress )
	( cd .distcheck/mdown-$(VERSION) && $(MAKE) install )
	rm -rf .distcheck

$(PDFS) index.xml README.xml: mdown

index.html README.html: template.xml

.md.pdf:
	./mdown --nroff-no-numbered -s -Tms $< | \
		pdfroff -i -mspdf -t -k > $@

index.latex.pdf: index.md $(THUMBS)
	./mdown -s -Tlatex index.md >index.latex.latex
	pdflatex index.latex.latex
	pdflatex index.latex.latex

index.mandoc.pdf: index.md
	./mdown --nroff-no-numbered -s -Tman index.md | \
		mandoc -Tpdf > $@

index.nroff.pdf: index.md
	./mdown --nroff-no-numbered -s -Tms index.md | \
		pdfroff -i -mspdf -t -k > $@

.xml.html:
	sblg -t template.xml -s date -o $@ -C $< $< versions.xml

archive.html: archive.xml versions.xml
	sblg -t archive.xml -s date -o $@ versions.xml

atom.xml: atom-template.xml versions.xml
	sblg -a -t atom-template.xml -s date -o $@ versions.xml

diff.html: diff.md mdown
	./mdown -s diff.md >$@

diff.diff.html: diff.md diff.old.md mdown-diff
	./mdown-diff -s diff.old.md diff.md >$@

diff.diff.pdf: diff.md diff.old.md mdown-diff
	./mdown-diff --nroff-no-numbered -s -Tms diff.old.md diff.md | \
		pdfroff -i -mspdf -t -k > $@

$(HTMLS): versions.xml

.md.xml: mdown
	( echo "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>" ; \
	  echo "<article data-sblg-article=\"1\">" ; \
	  ./mdown $< ; \
	  echo "</article>" ; ) >$@

.1.1.html .3.3.html .5.5.html:
	mandoc -Thtml -Ostyle=https://bsd.lv/css/mandoc.css $< >$@

mdown.tar.gz.sha512: mdown.tar.gz
	openssl dgst -sha512 -hex mdown.tar.gz >$@

mdown.tar.gz:
	mkdir -p .dist/mdown-$(VERSION)/
	mkdir -p .dist/mdown-$(VERSION)/man
	mkdir -p .dist/mdown-$(VERSION)/regress/MarkdownTest_1.0.3
	$(INSTALL) -m 0644 $(HEADERS) .dist/mdown-$(VERSION)
	$(INSTALL) -m 0644 $(SOURCES) .dist/mdown-$(VERSION)
	$(INSTALL) -m 0644 mdown.in.pc Makefile LICENSE.md .dist/mdown-$(VERSION)
	$(INSTALL) -m 0644 man/*.1 man/*.3 man/*.5 .dist/mdown-$(VERSION)/man
	$(INSTALL) -m 0755 configure .dist/mdown-$(VERSION)
	$(INSTALL) -m 644 regress/MarkdownTest_1.0.3/*.text \
		.dist/mdown-$(VERSION)/regress/MarkdownTest_1.0.3
	$(INSTALL) -m 644 regress/MarkdownTest_1.0.3/*.md \
		.dist/mdown-$(VERSION)/regress/MarkdownTest_1.0.3
	$(INSTALL) -m 644 regress/MarkdownTest_1.0.3/*.html \
		.dist/mdown-$(VERSION)/regress/MarkdownTest_1.0.3
	$(INSTALL) -m 644 regress/*.md \
		.dist/mdown-$(VERSION)/regress
	$(INSTALL) -m 644 regress/*.html \
		.dist/mdown-$(VERSION)/regress
	$(INSTALL) -m 644 regress/*.ms \
		.dist/mdown-$(VERSION)/regress
	$(INSTALL) -m 644 regress/*.man \
		.dist/mdown-$(VERSION)/regress
	$(INSTALL) -m 644 regress/*.latex \
		.dist/mdown-$(VERSION)/regress
	$(INSTALL) -m 644 regress/*.gemini \
		.dist/mdown-$(VERSION)/regress
	( cd .dist/ && tar zcf ../$@ mdown-$(VERSION) )
	rm -rf .dist/

$(OBJS) $(COMPAT_OBJS) main.o: config.h

$(OBJS): extern.h mdown.h

term.o: term.h

main.o: mdown.h

clean:
	rm -f $(OBJS) $(COMPAT_OBJS) main.o
	rm -f mdown mdown-diff libmdown.a mdown.pc
	rm -f index.xml diff.xml diff.diff.xml README.xml mdown.tar.gz.sha512 mdown.tar.gz
	rm -f $(PDFS) $(HTMLS) $(THUMBS)
	rm -f index.latex.aux index.latex.latex index.latex.log index.latex.out

distclean: clean
	rm -f Makefile.configure config.h config.log config.h.old config.log.old

regress: mdown
	tmp1=`mktemp` ; \
	tmp2=`mktemp` ; \
	for f in regress/MarkdownTest_1.0.3/*.text ; do \
		echo "$$f" ; \
		want="`dirname \"$$f\"`/`basename \"$$f\" .text`.html" ; \
		sed -e '/^[ ]*$$/d' "$$want" > $$tmp1 ; \
		./mdown $(REGRESS_ARGS) "$$f" | \
			sed -e 's!	! !g' | sed -e '/^[ ]*$$/d' > $$tmp2 ; \
		diff -uw $$tmp1 $$tmp2 ; \
		./mdown -s -Thtml "$$f" >/dev/null 2>&1 ; \
		./mdown -s -Tlatex "$$f" >/dev/null 2>&1 ; \
		./mdown -s -Tman "$$f" >/dev/null 2>&1 ; \
		./mdown -s -Tms "$$f" >/dev/null 2>&1 ; \
		./mdown -s -Tfodt "$$f" >/dev/null 2>&1 ; \
		./mdown -s -Tterm "$$f" >/dev/null 2>&1 ; \
		./mdown -s -Ttree "$$f" >/dev/null 2>&1 ; \
	done  ; \
	for f in regress/*.md ; do \
		echo "$$f" ; \
		if [ -f regress/`basename $$f .md`.html ]; then \
			./mdown -Thtml $$f >$$tmp1 2>&1 ; \
			diff -uw regress/`basename $$f .md`.html $$tmp1 ; \
		fi ; \
		if [ -f regress/`basename $$f .md`.latex ]; then \
			./mdown -Tlatex $$f >$$tmp1 2>&1 ; \
			diff -uw regress/`basename $$f .md`.latex $$tmp1 ; \
		fi ; \
		if [ -f regress/`basename $$f .md`.ms ]; then \
			./mdown -Tms $$f >$$tmp1 2>&1 ; \
			diff -uw regress/`basename $$f .md`.ms $$tmp1 ; \
		fi ; \
		if [ -f regress/`basename $$f .md`.man ]; then \
			./mdown -Tman $$f >$$tmp1 2>&1 ; \
			diff -uw regress/`basename $$f .md`.man $$tmp1 ; \
		fi ; \
		if [ -f regress/`basename $$f .md`.gemini ]; then \
			./mdown -Tgemini $$f >$$tmp1 2>&1 ; \
			diff -uw regress/`basename $$f .md`.gemini $$tmp1 ; \
		fi ; \
	done ; \
	for f in regress/standalone/*.md ; do \
		echo "$$f" ; \
		if [ -f regress/standalone/`basename $$f .md`.html ]; then \
			./mdown -s -Thtml $$f >$$tmp1 2>&1 ; \
			diff -uw regress/standalone/`basename $$f .md`.html $$tmp1 ; \
		fi ; \
		if [ -f regress/standalone/`basename $$f .md`.latex ]; then \
			./mdown -s -Tlatex $$f >$$tmp1 2>&1 ; \
			diff -uw regress/standalone/`basename $$f .md`.latex $$tmp1 ; \
		fi ; \
		if [ -f regress/standalone/`basename $$f .md`.ms ]; then \
			./mdown -s -Tms $$f >$$tmp1 2>&1 ; \
			diff -uw regress/standalone/`basename $$f .md`.ms $$tmp1 ; \
		fi ; \
		if [ -f regress/standalone/`basename $$f .md`.man ]; then \
			./mdown -s -Tman $$f >$$tmp1 2>&1 ; \
			diff -uw regress/standalone/`basename $$f .md`.man $$tmp1 ; \
		fi ; \
		if [ -f regress/standalone/`basename $$f .md`.gemini ]; then \
			./mdown -s -Tgemini $$f >$$tmp1 2>&1 ; \
			diff -uw regress/standalone/`basename $$f .md`.gemini $$tmp1 ; \
		fi ; \
	done ; \
	for f in regress/metadata/*.md ; do \
		echo "$$f" ; \
		if [ -f regress/metadata/`basename $$f .md`.txt ]; then \
			./mdown -X test $$f >$$tmp1 2>&1 ; \
			diff -uw regress/metadata/`basename $$f .md`.txt $$tmp1 ; \
		fi ; \
	done ; \
	rm -f $$tmp1 ; \
	rm -f $$tmp2

.png.thumb.jpg:
	convert $< -thumbnail 350 -quality 50 $@

.in.pc.pc:
	sed -e "s!@PREFIX@!$(PREFIX)!g" \
	    -e "s!@LIBDIR@!$(LIBDIR)!g" \
	    -e "s!@INCLUDEDIR@!$(INCLUDEDIR)!g" \
	    -e "s!@VERSION@!$(VERSION)!g" $< >$@

force:
	make
	doas make install
