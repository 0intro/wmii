# window manager improved 2 user guide
SRC = guide_en.tex wmii.tex
Odvi = ${SRC:.tex=.dvi}
Opdf = ${SRC:.tex=.pdf}
Ohtml = ${SRC:.tex=.html}

all: ${Odvi} ${Opdf} #${Ohtml}
%.dvi: %.tex
	latex -interaction=batchmode $<
	latex -interaction=batchmode $<
%.pdf: %.tex
	pdflatex $<
	pdflatex $<
%.html: %.tex
	latex2html $<
clean:
	rm -f *.pdf *.dvi *.log *.aux *.out *.toc
	rm -rf guide_en
