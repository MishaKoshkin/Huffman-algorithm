#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Максимальное количество различных символов (256 для байтов)
#define MAX_SYMBOLS 256
// Максимальный размер кучи для дерева Хаффмана
#define MAX_HEAP_SIZE 256

// Структура для узла дерева Хаффмана
typedef struct Node {
    unsigned char symbol; // Символ (байт)
    unsigned long long freq; // Частота символа
    struct Node *left, *right; // Левый и правый потомки
} Node;

// Структура для приоритетной очереди (min-heap)
typedef struct {
    Node* nodes[MAX_HEAP_SIZE];
    int size;
} MinHeap;

// Структура для хранения кода Хаффмана
typedef struct {
    char code[256]; // Код для символа (битовая строка)
    int length; // Длина кода
} Code;

// Инициализация кучи
void init_heap(MinHeap* heap) {
    heap->size = 0;
}

// Добавление узла в кучу
void insert_heap(MinHeap* heap, Node* node) {
    int i = heap->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap->nodes[parent]->freq <= node->freq) break;
        heap->nodes[i] = heap->nodes[parent];
        i = parent;
    }
    heap->nodes[i] = node;
}

// Извлечение узла с минимальной частотой
Node* extract_min(MinHeap* heap) {
    Node* min = heap->nodes[0];
    heap->nodes[0] = heap->nodes[--heap->size];
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        if (left < heap->size && heap->nodes[left]->freq < heap->nodes[smallest]->freq)
            smallest = left;
        if (right < heap->size && heap->nodes[right]->freq < heap->nodes[smallest]->freq)
            smallest = right;
        if (smallest == i) break;
        Node* temp = heap->nodes[i];
        heap->nodes[i] = heap->nodes[smallest];
        heap->nodes[smallest] = temp;
        i = smallest;
    }
    return min;
}

// Создание узла дерева Хаффмана
Node* create_node(unsigned char symbol, unsigned long long freq) {
    Node* node = (Node*)malloc(sizeof(Node));
    node->symbol = symbol;
    node->freq = freq;
    node->left = node->right = NULL;
    return node;
}

// Построение дерева Хаффмана
Node* build_huffman_tree(unsigned long long* freq) {
    MinHeap heap;
    init_heap(&heap);
    // Добавляем все символы с ненулевой частотой в кучу
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (freq[i] > 0) {
            insert_heap(&heap, create_node((unsigned char)i, freq[i]));
        }
    }
    // Строим дерево, объединяя узлы
    while (heap.size > 1) {
        Node* left = extract_min(&heap);
        Node* right = extract_min(&heap);
        Node* parent = create_node(0, left->freq + right->freq);
        parent->left = left;
        parent->right = right;
        insert_heap(&heap, parent);
    }
    return heap.size == 0 ? NULL : extract_min(&heap);
}

// Рекурсивное построение кодов Хаффмана
void build_codes(Node* root, Code* codes, char* current_code, int depth) {
    if (!root) return;
    if (!root->left && !root->right) {
        codes[root->symbol].length = depth;
        strcpy(codes[root->symbol].code, current_code);
        return;
    }
    current_code[depth] = '0';
    build_codes(root->left, codes, current_code, depth + 1);
    current_code[depth] = '1';
    build_codes(root->right, codes, current_code, depth + 1);
}

// Освобождение памяти для дерева Хаффмана
void free_tree(Node* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

// Кодирование файла
void encode_file(const char* input_file, const char* output_file) {
    unsigned long long freq[MAX_SYMBOLS] = {0};
    FILE* in = fopen(input_file, "rb");
    if (!in) {
        printf("Error opening file %s\n", input_file);
        exit(1);
    }
    // Подсчёт частот байтов
    int c;
    unsigned long long total_bits = 0; // Считаем общее количество битов
    while ((c = fgetc(in)) != EOF) {
        freq[(unsigned char)c]++;
    }
    fseek(in, 0, SEEK_SET);

    // Построение дерева Хаффмана
    Node* root = build_huffman_tree(freq);
    if (!root) {
        printf("Empty file\n");
        fclose(in);
        return;
    }

    // Построение кодов
    Code codes[MAX_SYMBOLS];
    char current_code[256] = {0};
    for (int i = 0; i < MAX_SYMBOLS; i++) codes[i].length = 0;
    build_codes(root, codes, current_code, 0);

    // Вычисляем общее количество битов для кодирования
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (freq[i] > 0) {
            total_bits += freq[i] * codes[i].length;
        }
    }

    // Запись закодированного файла
    FILE* out = fopen(output_file, "wb"); // Открываем файл для записи в бинарном режиме
    if (!out) {
        printf("Error opening file %s\n", output_file);
        fclose(in);
        free_tree(root);
        exit(1);
    }

    // Записываем заголовок: количество символов, частоты, количество валидных битов и общее количество битов
    int symbol_count = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (freq[i] > 0) symbol_count++;
    }
    fwrite(&symbol_count, sizeof(int), 1, out);
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (freq[i] > 0) {
            fwrite(&i, sizeof(unsigned char), 1, out);
            fwrite(&freq[i], sizeof(unsigned long long), 1, out);
        }
    }
    unsigned char valid_bits = total_bits % 8 ? total_bits % 8 : 8; // Количество валидных битов в последнем байте
    fwrite(&valid_bits, sizeof(unsigned char), 1, out);
    fwrite(&total_bits, sizeof(unsigned long long), 1, out);

    // Записываем закодированные данные
    unsigned char buffer = 0;
    int bit_count = 0;
    while ((c = fgetc(in)) != EOF) {
        unsigned char symbol = (unsigned char)c;
        for (int i = 0; i < codes[symbol].length; i++) {
            buffer = (buffer << 1) | (codes[symbol].code[i] - '0');
            bit_count++;
            if (bit_count == 8) {
                fwrite(&buffer, sizeof(unsigned char), 1, out);
                buffer = 0;
                bit_count = 0;
            }
        }
    }
    // Записываем оставшиеся биты
    if (bit_count > 0) {
        buffer <<= (8 - bit_count);
        fwrite(&buffer, sizeof(unsigned char), 1, out);
    }

    fclose(in);
    fclose(out);
    free_tree(root);
}

// Декодирование файла
void decode_file(const char* input_file, const char* output_file) {
    FILE* in = fopen(input_file, "rb");
    if (!in) {
        printf("Error opening file %s\n", input_file);
        exit(1);
    }

    // Читаем заголовок
    int symbol_count;
    fread(&symbol_count, sizeof(int), 1, in);
    unsigned long long freq[MAX_SYMBOLS] = {0};
    for (int i = 0; i < symbol_count; i++) {
        unsigned char symbol;
        unsigned long long f;
        fread(&symbol, sizeof(unsigned char), 1, in);
        fread(&f, sizeof(unsigned long long), 1, in);
        freq[symbol] = f;
    }
    unsigned char valid_bits;
    fread(&valid_bits, sizeof(unsigned char), 1, in); // Читаем количество валидных битов
    unsigned long long total_bits;
    fread(&total_bits, sizeof(unsigned long long), 1, in); // Читаем общее количество битов

    // Восстанавливаем дерево Хаффмана
    Node* root = build_huffman_tree(freq);
    if (!root) {
        printf("Empty file\n");
        fclose(in);
        return;
    }

    // Открываем файл для записи результата
    FILE* out = fopen(output_file, "wb");
    if (!out) {
        printf("Error opening file %s\n", output_file);
        fclose(in);
        free_tree(root);
        exit(1);
    }

    // Декодируем данные
    Node* current = root;
    unsigned char buffer;
    unsigned long long bit_count = 0;
    int c;
    while ((c = fgetc(in)) != EOF && bit_count < total_bits) {
        buffer = (unsigned char)c;
        int bits_to_read = (bit_count + 8 > total_bits) ? (total_bits - bit_count) : 8;
        if (feof(in)) break; // Проверяем конец файла
        for (int i = 7; i >= 8 - bits_to_read; i--) {
            int bit = (buffer >> i) & 1;
            current = bit ? current->right : current->left;
            if (!current->left && !current->right) {
                fputc(current->symbol, out);
                current = root;
            }
            bit_count++;
            if (bit_count >= total_bits) break;
        }
    }

    fclose(in);
    fclose(out);
    free_tree(root);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Wrong parameter: use 'encode' or 'decode'\n");
        return 1;
    }

    if (strcmp(argv[1], "encode") == 0) {
        encode_file("input.txt", "encoded.bin");
    } else if (strcmp(argv[1], "decode") == 0) {
        decode_file("encoded.bin", "decoded.txt");
    } else {
        printf("Wrong parameter: use 'encode' or 'decode'\n");
        return 1;
    }

    return 0;
}