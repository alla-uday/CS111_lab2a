default:
	gcc -pthread -o lab2a lab2a.c
clean:
	$(RM) lab2a *.o *~
dist:
	tar -cvzf lab2a-404428077.tar.gz lab2a.c README Makefile synchronizations.png costperop.png
