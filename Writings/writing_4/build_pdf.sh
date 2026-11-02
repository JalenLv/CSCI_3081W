#!/bin/bash
set -e
cd "$(dirname "$0")"

latexmk -xelatex -interaction=nonstopmode -halt-on-error writing_4.tex
latexmk -xelatex -c writing_4.tex
