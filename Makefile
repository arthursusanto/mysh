CFLAGS = -g -Wall -Wextra -Werror -fsanitize=address,leak,object-size,bounds-strict,undefined -fsanitize-address-use-after-scope

all: mysh

mysh: mysh.o builtins.o variables.o io_helpers.o 
	gcc ${CFLAGS} -o $@ $^ 

%.o: %.c builtins.h variables.h io_helpers.h 
	gcc ${CFLAGS} -c $< 

clean:
	rm *.o mysh
