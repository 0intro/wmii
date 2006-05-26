# window manager improved 2 user guide
l2h_opts=-local_icons -scalable_fonts -split +1 -subdir -show_section_numbers -address "<br>Last update: $(shell date) by $$USER@$$HOSTNAME"

SRC = guide_en.tex wmii.tex
DVI = ${SRC:.tex=.dvi}
PDF = ${SRC:.tex=.pdf}
PS = ${SRC:.tex=.ps}
HTML = ${SRC:.tex=.html}

.SUFFIXES: .tex .dvi .pdf .ps .html

all: ${DVI} ${PDF} #${PS} ${HTML} 
.tex.dvi:
	latex -interaction=batchmode $<
	latex -interaction=batchmode $<
.tex.pdf:
	pdflatex $<
	pdflatex $<
.dvi.ps:
	dvips -o $@ $<
.tex.html:
	latex2html ${l2h_opts} $<
clean:
	rm -f *.pdf *.ps *.dvi *.log *.aux *.out *.toc
	rm -rf guide_en
