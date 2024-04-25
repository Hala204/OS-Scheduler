build:
	gcc process_generator.c -o process_generator.out
	gcc clk.c -o test_generator.out
	gcc scheduler.c  hashmap.c -lm priority_queue.c -o scheduler.out
	gcc process.c -o process.out
	gcc test_generator.c -o test_generator.out

clean:
	rm -f *.out  processes.txt


run:
	./process_generator.out 1 4

generate:
	gcc test_generator.c -o test_generator.out
	./test_generator.out

test: build
	./process_generator.out 3 1

buddy:
	gcc -o hey -g  buddy_core.c -lm
	./hey

