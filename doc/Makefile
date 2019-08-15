default: reglan.pdf

reglan.pdf: Doxyfile ../src/* ../python/*
	doxygen Doxyfile
	cd latex && (make && cp refman.pdf ../reglan.pdf; cd ..)

Doxyfile: Doxyfile.in subs.py ../src/reglan.h
	python subs.py

clean:
	rm -rf html latex reglan.pdf Doxyfile