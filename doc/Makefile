# window manager improved 2 user guide

SRC = guide_en.tex wmii.tex
DVI = ${SRC:.tex=.dvi}
PDF = ${SRC:.tex=.pdf}
HTML = ${SRC:.tex=.html}

all: ${DVI} ${PDF} #${HTML}

.SUFFIXES: .tex .dvi .pdf .html

.tex.dvi:
	latex -interaction=batchmode $<
	latex -interaction=batchmode $<

.tex.pdf:
	pdflatex $<
	pdflatex $<

.tex.html:
	latex2html $<

clean:
	rm -f *.pdf *.dvi *.log *.aux *.out *.toc
	rm -rf guide_en
