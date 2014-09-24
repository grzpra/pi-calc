CROSS_COMPILE?=

CC=$(CROSS_COMPILE)gcc

APP_NAME=pi-calc
SRC_DIR=src
INC_DIR=inc
INT_DIR=int

CFLAGS=-I $(INC_DIR) -O2
LDFLAGS=-lgmp -lpthread

OBJECTS=$(patsubst $(SRC_DIR)/%.c,$(INT_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))

all: $(OBJECTS)
	@echo " [LD] $?"
	@$(CC) $^ -o $(APP_NAME) $(LDFLAGS)

$(INT_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(INT_DIR)
	@echo " [CC] $?"
	@$(CC) $< -o $@ -c $(CFLAGS)

clean:
	@$(RM) -rf $(INT_DIR) $(APP_NAME)
