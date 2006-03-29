SRC = guide_en.tex wmii.tex
Odvi = ${SRC:.tex=.dvi}
Opdf = ${SRC:.tex=.pdf}

all: ${Odvi} ${Opdf}
	@echo Finished doc
	@echo -------------------------------------------------------
%.dvi: %.tex
	latex -interaction=batchmode $<
	latex -interaction=batchmode $<
%.pdf: %.tex
	pdflatex $<
clean:
	rm -f *.pdf *.dvi *.log *.aux *.out *.toc
