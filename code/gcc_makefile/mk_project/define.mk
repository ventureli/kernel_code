CC := gcc
CFLAGS := -Wall -g -Iinclude
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

TARGET := $(BIN_DIR)/app
 
SRCS := $(wildcard $(SRC_DIR)/*.c)

OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all info clean
