include define.mk

info:
	@echo "Source list:"$(SRCS)
	@echo "Object list:"$(OBJS)

all:$(TARGET)

$(TARGET): $(OBJS)
	@echo "Source list:"$(SRCS)
	@echo "Object list:"$(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)