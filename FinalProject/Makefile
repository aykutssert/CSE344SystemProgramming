
compile:
	gcc PideShop.c -o PideShop -lpthread -lm
	gcc HungryVeryMuch.c -o HungryVeryMuch

server: 
	./PideShop 192.168.233.3 8080 4 6 1 
	
client:
	./HungryVeryMuch 192.168.233.3  8080 10  10 20
	
clean:
	rm *.log ./PideShop ./HungryVeryMuch