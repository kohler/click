LATEX	= latex
BIBTEX	= bibtex
METAPOST= mpost
DVIPS	= dvips

FIGS = samp01_examples.1 samp01_examples.2 samp01_examples.3 samp01_examples.4 samp01_examples.5 samp01_examples.6 samp01_examples.7 samp01_examples.8 \
	samp02_processing.1 samp02_processing.2 samp02_processing.3 samp02_processing.4 \
	samp03_routers.1 samp03_routers.2 samp03_routers.3 samp03_routers.4 \
	samp04_iprouter.1 \
	samp05_other.1 samp05_other.2 samp05_other.3 samp05_other.4 samp05_other.5

all: $(FIGS)

# after making the figure, run perl to fix Computer Modern font names to
# uppercase
%.1 %.2 %.3 %.4 %.5 %.6 %.7 %.8 %.9: %.mp elements.mp elemfig.sty
	TEX=$(LATEX) $(METAPOST) $<
	perl -pi -e 'use bytes; next if $$x eq $$ARGV; $$x = $$ARGV if /%%EndProlog/; s=/([a-z][a-z0-9]+) def=/\U$$1\E def= if !/^%%/; s=\b([a-z][a-z0-9]+)\b=\U$$1\E=g if /^%%DocumentFonts/ || /^%%\+/;' $(subst .mp,,$<).[0-9]*

always:
	@:

clean:
	/bin/rm -f $(FIGS) *.log *.aux *.dvi *.mpx

.PHONY: all bib clean cleanfig clean-all always
