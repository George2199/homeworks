/*
 * ПР №1. Связный линейный список. Вариант 10. Уровень: Повышенный.
 *
 * Предметная область: библиотека книг.
 * Поля данных: фамилия автора, название, издательство, год издания,
 *   тематика, количество экземпляров.
 * Union-поле (доп. сведения по типу книги):
 *   SCIENTIFIC — УДК, кол-во источников
 *   FICTION    — жанр, серия
 *   TEXTBOOK   — дисциплина, уровень (школьный/вузовский)
 *
 * Запросы варианта:
 *   1) Авторы и названия книг заданного издательства за последние 5 лет.
 *   2) Доля книг заданной тематики от общего числа экземпляров.
 *
 * Требования уровня «Повышенный»:
 *   - Двусвязный список; узлы хранят только адрес отдельно выделенных данных.
 *   - Имя файла — argv[1]; если не задано — с клавиатуры.
 *   - Сохранение при выходе в тот же бинарный файл.
 *   - Постраничный вывод (N/P/Q) с листанием в обоих направлениях.
 *   - Вспомогательные двусвязные списки указателей для сортировки и фильтрации.
 *   - Двухуровневое меню.
 *   - Функции сортировки и удаления принимают компаратор/предикат параметром.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#  include <windows.h>
#  define CLRSCR()    system("cls")
#  define ICMP(a, b)  _stricmp((a), (b))
#else
#  include <strings.h>
#  define CLRSCR()    system("clear")
#  define ICMP(a, b)  strcasecmp((a), (b))
#endif

#define PAGE_SIZE    5
#define DEFAULT_FILE "library.dat"

/* Текущий год (не константа — вычисляется в runtime) */
static int cur_year(void) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    return tm->tm_year + 1900;
}


/* ═══════════════════════════════════════════════════════════
 *                      ТИПЫ ДАННЫХ
 * ═══════════════════════════════════════════════════════════ */

typedef enum { SCIENTIFIC = 0, FICTION = 1, TEXTBOOK = 2 } BookKind;

/* Вариативные поля (union): активный вариант задаётся полем kind */
union BookExtra {
    struct { char udk[20]; int sources; }       sci; /* научная:       УДК, источники  */
    struct { char genre[30]; char series[40]; } fic; /* худ.:          жанр, серия     */
    struct { char discipline[40]; int level; }  txt; /* учебник:       предмет, уровень*/
};

typedef struct {
    char           author[50];     /* фамилия автора              */
    char           title[80];      /* название                    */
    char           publisher[50];  /* издательство                */
    int            year;           /* год издания                 */
    char           topic[40];      /* тематика                    */
    int            copies;         /* количество экземпляров      */
    BookKind       kind;           /* тип — ключ к union          */
    union BookExtra extra;         /* вариативные поля            */
} Book;


/* ═══════════════════════════════════════════════════════════
 *  ОСНОВНОЙ ДВУСВЯЗНЫЙ СПИСОК
 *  Узел хранит только указатель на отдельно выделенный Book.
 * ═══════════════════════════════════════════════════════════ */

typedef struct DNode {
    Book         *data;
    struct DNode *prev, *next;
} DNode;

typedef struct {
    DNode  *head, *tail;
    size_t  count;
} DList;

static void dlist_init(DList *L) {
    L->head = L->tail = NULL; L->count = 0;
}

/* Добавить в конец — O(1) благодаря указателю tail */
static DNode *dlist_append(DList *L, Book *b) {
    DNode *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->data = b; n->next = NULL; n->prev = L->tail;
    if (L->tail) L->tail->next = n; else L->head = n;
    L->tail = n; L->count++;
    return n;
}

/* Включить в упорядоченный список — компаратор передаётся параметром */
static DNode *dlist_insert_sorted(DList *L, Book *b,
        int (*cmp)(const Book *, const Book *)) {
    DNode *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->data = b;
    if (!L->head) {
        n->prev = n->next = NULL;
        L->head = L->tail = n; L->count++;
        return n;
    }
    /* Ищем первый узел, который уже «больше» b */
    DNode *cur = L->head;
    while (cur && cmp(cur->data, b) <= 0) cur = cur->next;
    if (!cur) {                      /* вставить в конец */
        n->next = NULL; n->prev = L->tail;
        L->tail->next = n; L->tail = n;
    } else if (!cur->prev) {         /* вставить в начало */
        n->prev = NULL; n->next = L->head;
        L->head->prev = n; L->head = n;
    } else {                         /* вставить перед cur */
        n->prev = cur->prev; n->next = cur;
        cur->prev->next = n; cur->prev = n;
    }
    L->count++;
    return n;
}

/* Удалить первый элемент, для которого pred(data, arg) != 0.
   Предикат передаётся параметром. Освобождает узел и данные. */
static int dlist_remove(DList *L,
        int (*pred)(const Book *, const void *), const void *arg) {
    for (DNode *c = L->head; c; c = c->next) {
        if (!pred(c->data, arg)) continue;
        if (c->prev) c->prev->next = c->next; else L->head = c->next;
        if (c->next) c->next->prev = c->prev; else L->tail = c->prev;
        free(c->data); free(c); L->count--;
        return 1;
    }
    return 0;
}

static void dlist_free(DList *L) {
    for (DNode *c = L->head; c; ) {
        DNode *t = c->next;
        free(c->data); free(c);
        c = t;
    }
    dlist_init(L);
}


/* ═══════════════════════════════════════════════════════════
 *  ВСПОМОГАТЕЛЬНЫЙ ДВУСВЯЗНЫЙ СПИСОК УКАЗАТЕЛЕЙ
 *  Узел хранит Book *ref — адрес данных из основного списка.
 *  Данные НЕ освобождаются при уничтожении списка.
 * ═══════════════════════════════════════════════════════════ */

typedef struct AuxNode {
    Book          *ref;
    struct AuxNode *prev, *next;
} AuxNode;

typedef struct {
    AuxNode *head, *tail;
    size_t   count;
} AuxList;

static void aux_init(AuxList *A) { A->head = A->tail = NULL; A->count = 0; }

static AuxNode *aux_append(AuxList *A, Book *ref) {
    AuxNode *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->ref = ref; n->next = NULL; n->prev = A->tail;
    if (A->tail) A->tail->next = n; else A->head = n;
    A->tail = n; A->count++;
    return n;
}

/* Вставка в упорядоченный вспомогательный список — компаратор параметром */
static AuxNode *aux_insert_sorted(AuxList *A, Book *ref,
        int (*cmp)(const Book *, const Book *)) {
    AuxNode *n = malloc(sizeof *n);
    if (!n) return NULL;
    n->ref = ref;
    if (!A->head) {
        n->prev = n->next = NULL;
        A->head = A->tail = n; A->count++;
        return n;
    }
    AuxNode *cur = A->head;
    while (cur && cmp(cur->ref, ref) <= 0) cur = cur->next;
    if (!cur) {
        n->next = NULL; n->prev = A->tail;
        A->tail->next = n; A->tail = n;
    } else if (!cur->prev) {
        n->prev = NULL; n->next = A->head;
        A->head->prev = n; A->head = n;
    } else {
        n->prev = cur->prev; n->next = cur;
        cur->prev->next = n; cur->prev = n;
    }
    A->count++;
    return n;
}

static void aux_free(AuxList *A) {
    for (AuxNode *c = A->head; c; ) {
        AuxNode *t = c->next; free(c); c = t;
    }
    aux_init(A);
}


/* ═══════════════════════════════════════════════════════════
 *                   КОНСОЛЬНЫЙ ВВОД
 * ═══════════════════════════════════════════════════════════ */

static void read_str(const char *prompt, char *buf, int maxlen) {
    printf("%s: ", prompt);
    fflush(stdout);
    if (!fgets(buf, maxlen, stdin)) { buf[0] = '\0'; return; }
    char *p = strchr(buf, '\n');
    if (p) *p = '\0';
    else { int c; while ((c = getchar()) != '\n' && c != EOF); }
}

/* Чтение целого числа через fgets — не смешивает буферы со scanf */
static int read_int(const char *prompt) {
    char buf[32];
    int v = 0;
    for (;;) {
        printf("%s: ", prompt);
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) return 0;
        if (sscanf(buf, "%d", &v) == 1) return v;
        puts("  Введите целое число.");
    }
}

static void pause_enter(void) {
    printf("Нажмите Enter...");
    fflush(stdout);
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

static const char *kind_name(BookKind k) {
    switch (k) {
        case SCIENTIFIC: return "Науч.";
        case FICTION:    return "Худ.";
        case TEXTBOOK:   return "Учеб.";
        default:         return "???";
    }
}

/* ─── UTF-8-aware padding ────────────────────────────────────
 * Выводит строку s, занимая ровно vis_width «видимых» символов.
 * Если строка длиннее — обрезается по границе символа UTF-8.
 * Если короче — дополняется пробелами.
 * ──────────────────────────────────────────────────────────── */
static void u8pad(const char *s, int vis_width) {
    const unsigned char *p = (const unsigned char *)s;
    int vis = 0, bytes = 0;
    while (*p && vis < vis_width) {
        int cb = (*p < 0x80) ? 1 :
                 (*p < 0xE0) ? 2 :
                 (*p < 0xF0) ? 3 : 4;
        vis++; bytes += cb; p += cb;
    }
    printf("%.*s", bytes, s);
    for (int i = vis; i < vis_width; i++) putchar(' ');
}


/* ═══════════════════════════════════════════════════════════
 *  ТАБЛИЦА
 *  Колонки (визуальных символов):
 *    N(2)  Автор(18)  Название(24)  Изд-во(14)  Год(4)  Тема(12)  Экз(5)  Тип(5)
 * ═══════════════════════════════════════════════════════════ */

#define HLINE \
    "+----+--------------------+--------------------------+" \
    "----------------+------+--------------+-------+-------+"

static void print_table_header(void) {
    puts(HLINE);
    fputs("| ", stdout); u8pad("N",            2);
    fputs(" | ", stdout); u8pad("Автор",       18);
    fputs(" | ", stdout); u8pad("Название",    24);
    fputs(" | ", stdout); u8pad("Издательство",14);
    fputs(" | ", stdout); u8pad("Год",          4);
    fputs(" | ", stdout); u8pad("Тематика",    12);
    fputs(" | ", stdout); u8pad("Экз.",         5);
    fputs(" | ", stdout); u8pad("Тип",          5);
    puts(" |");
    puts(HLINE);
}

static void print_book_row(int n, const Book *b) {
    char nbuf[8], ybuf[8], cbuf[8];
    snprintf(nbuf, sizeof nbuf, "%2d",  n);
    snprintf(ybuf, sizeof ybuf, "%4d",  b->year);
    snprintf(cbuf, sizeof cbuf, "%5d",  b->copies);
    fputs("| ", stdout); u8pad(nbuf,            2);
    fputs(" | ", stdout); u8pad(b->author,      18);
    fputs(" | ", stdout); u8pad(b->title,       24);
    fputs(" | ", stdout); u8pad(b->publisher,   14);
    fputs(" | ", stdout); fputs(ybuf, stdout);
    fputs(" | ", stdout); u8pad(b->topic,       12);
    fputs(" | ", stdout); fputs(cbuf, stdout);
    fputs(" | ", stdout); u8pad(kind_name(b->kind), 5);
    puts(" |");
}

/* Вывод вариативных (union) полей под строкой таблицы */
static void print_book_extra(int n, const Book *b) {
    printf("  [%d] ", n);
    switch (b->kind) {
        case SCIENTIFIC:
            fputs("УДК: ", stdout); u8pad(b->extra.sci.udk, 12);
            printf(" | Источников: %d\n", b->extra.sci.sources);
            break;
        case FICTION:
            fputs("Жанр: ", stdout); u8pad(b->extra.fic.genre, 14);
            fputs(" | Серия: ", stdout);
            puts(b->extra.fic.series);
            break;
        case TEXTBOOK:
            fputs("Дисц.: ", stdout); u8pad(b->extra.txt.discipline, 13);
            printf(" | Уровень: %s\n",
                   b->extra.txt.level ? "вузовский" : "школьный");
            break;
    }
}


/* ═══════════════════════════════════════════════════════════
 *  ПОСТРАНИЧНЫЙ ВЫВОД ИЗ ВСПОМОГАТЕЛЬНОГО СПИСКА
 *  [N] — следующая страница, [P] — предыдущая, [Q] — выход.
 * ═══════════════════════════════════════════════════════════ */

static void paged_view(AuxList *A, const char *title) {
    if (!A->count) { puts("Список пуст."); pause_enter(); return; }

    int page  = 0;
    int pages = (int)((A->count + PAGE_SIZE - 1) / PAGE_SIZE);
    char ch[8];

    for (;;) {
        CLRSCR();
        printf("=== %s  [стр. %d / %d,  всего: %zu] ===\n",
               title, page + 1, pages, A->count);
        print_table_header();

        /* Смещаемся к началу нужной страницы */
        AuxNode *cur = A->head;
        int start = page * PAGE_SIZE;
        for (int i = 0; i < start && cur; i++) cur = cur->next;

        /* Сохраняем начало страницы для второго прохода (extra-поля) */
        AuxNode *page_start = cur;
        int row = start;
        for (int i = 0; i < PAGE_SIZE && cur; i++, cur = cur->next)
            print_book_row(++row, cur->ref);
        puts(HLINE);

        /* Вариативные поля под таблицей */
        cur = page_start; row = start;
        for (int i = 0; i < PAGE_SIZE && cur; i++, cur = cur->next)
            print_book_extra(++row, cur->ref);

        printf("\n[N]-Вперёд  [P]-Назад  [Q]-Выход > ");
        fflush(stdout);
        fgets(ch, sizeof ch, stdin);

        if      (ch[0]=='n' || ch[0]=='N') { if (page < pages-1) page++; }
        else if (ch[0]=='p' || ch[0]=='P') { if (page > 0)       page--; }
        else if (ch[0]=='q' || ch[0]=='Q') break;
    }
}

/* Обход основного списка вперёд (head → tail) */
static void view_forward(DList *L) {
    AuxList A; aux_init(&A);
    for (DNode *c = L->head; c; c = c->next) aux_append(&A, c->data);
    paged_view(&A, "Просмотр (вперёд)");
    aux_free(&A);
}

/* Обход основного списка назад (tail → head) — используем указатель prev */
static void view_backward(DList *L) {
    AuxList A; aux_init(&A);
    for (DNode *c = L->tail; c; c = c->prev) aux_append(&A, c->data);
    paged_view(&A, "Просмотр (назад)");
    aux_free(&A);
}


/* ═══════════════════════════════════════════════════════════
 *                      КОМПАРАТОРЫ
 * ═══════════════════════════════════════════════════════════ */

static int cmp_author   (const Book *a, const Book *b) { return strcmp(a->author,    b->author);    }
static int cmp_title    (const Book *a, const Book *b) { return strcmp(a->title,     b->title);     }
static int cmp_year_asc (const Book *a, const Book *b) { return a->year    - b->year;               }
static int cmp_year_desc(const Book *a, const Book *b) { return b->year    - a->year;               }
static int cmp_publisher(const Book *a, const Book *b) { return strcmp(a->publisher, b->publisher); }
static int cmp_topic    (const Book *a, const Book *b) { return strcmp(a->topic,     b->topic);     }
static int cmp_copies   (const Book *a, const Book *b) { return a->copies  - b->copies;             }

/* Строит отсортированный вспомогательный список и показывает постранично */
static void show_sorted(DList *L, int (*cmp)(const Book *, const Book *),
                        const char *title) {
    AuxList A; aux_init(&A);
    for (DNode *c = L->head; c; c = c->next)
        aux_insert_sorted(&A, c->data, cmp);
    paged_view(&A, title);
    aux_free(&A);
}


/* ═══════════════════════════════════════════════════════════
 *                      ПРЕДИКАТЫ
 * (для dlist_remove — предикат передаётся параметром)
 * ═══════════════════════════════════════════════════════════ */

static int pred_author(const Book *b, const void *arg) {
    return ICMP(b->author, (const char *)arg) == 0;
}
static int pred_title(const Book *b, const void *arg) {
    return ICMP(b->title,  (const char *)arg) == 0;
}


/* ═══════════════════════════════════════════════════════════
 *                   ФАЙЛОВЫЕ ОПЕРАЦИИ
 * ═══════════════════════════════════════════════════════════ */

static void load_file(DList *L, const char *fname) {
    FILE *f = fopen(fname, "rb");
    if (!f) {
        printf("Файл '%s' не найден — начинаем с пустого списка.\n", fname);
        return;
    }
    Book tmp;
    while (fread(&tmp, sizeof tmp, 1, f) == 1) {
        Book *b = malloc(sizeof *b);
        if (!b) { perror("malloc"); break; }
        *b = tmp;
        dlist_append(L, b);
    }
    fclose(f);
    printf("Загружено %zu записей из '%s'.\n", L->count, fname);
}

static void save_file(DList *L, const char *fname) {
    FILE *f = fopen(fname, "wb");
    if (!f) { perror("Ошибка сохранения"); return; }
    size_t n = 0;
    for (DNode *c = L->head; c; c = c->next) {
        fwrite(c->data, sizeof(Book), 1, f);
        n++;
    }
    fclose(f);
    printf("Сохранено %zu записей в '%s'.\n", n, fname);
}


/* ═══════════════════════════════════════════════════════════
 *                      ВВОД КНИГИ
 * ═══════════════════════════════════════════════════════════ */

static Book *input_book(void) {
    Book *b = malloc(sizeof *b);
    if (!b) return NULL;
    CLRSCR();
    puts("=== Добавление книги ===");
    read_str("Фамилия автора",      b->author,    sizeof b->author);
    read_str("Название",            b->title,     sizeof b->title);
    read_str("Издательство",        b->publisher, sizeof b->publisher);
    b->year   = read_int("Год издания");
    read_str("Тематика",            b->topic,     sizeof b->topic);
    b->copies = read_int("Кол-во экземпляров");
    if (b->copies <= 0) b->copies = 1;

    puts("Тип:  0 — Научная   1 — Художественная   2 — Учебник");
    int k = read_int("Тип");
    b->kind = (k >= 0 && k <= 2) ? (BookKind)k : SCIENTIFIC;

    memset(&b->extra, 0, sizeof b->extra);
    switch (b->kind) {
        case SCIENTIFIC:
            read_str("УДК",               b->extra.sci.udk,        sizeof b->extra.sci.udk);
            b->extra.sci.sources = read_int("Кол-во источников");
            break;
        case FICTION:
            read_str("Жанр",  b->extra.fic.genre,  sizeof b->extra.fic.genre);
            read_str("Серия", b->extra.fic.series, sizeof b->extra.fic.series);
            break;
        case TEXTBOOK:
            read_str("Дисциплина", b->extra.txt.discipline, sizeof b->extra.txt.discipline);
            b->extra.txt.level =
                (read_int("Уровень (0 — школьный, 1 — вузовский)") == 1) ? 1 : 0;
            break;
    }
    return b;
}


/* ═══════════════════════════════════════════════════════════
 *               ЗАПРОСЫ ВАРИАНТА 10
 * ═══════════════════════════════════════════════════════════ */

/* Запрос 1: авторы и названия книг заданного издательства за последние 5 лет */
static void query_publisher_5years(DList *L) {
    CLRSCR();
    puts("=== Запрос 1: книги издательства за последние 5 лет ===");
    char pub[50];
    read_str("Издательство", pub, sizeof pub);

    int yr = cur_year();
    AuxList A; aux_init(&A);
    for (DNode *c = L->head; c; c = c->next)
        if (ICMP(c->data->publisher, pub) == 0 &&
            c->data->year >= yr - 5 &&
            c->data->year <= yr)
            aux_append(&A, c->data);

    if (!A.count) {
        printf("Книг издательства «%s» за %d–%d не найдено.\n", pub, yr-5, yr);
        pause_enter();
    } else {
        char title[128];
        snprintf(title, sizeof title,
                 "Изд-во «%s», %d–%d (%zu шт.)", pub, yr-5, yr, A.count);
        paged_view(&A, title);
    }
    aux_free(&A);
}

/* Запрос 2: доля экземпляров заданной тематики от общего числа экземпляров */
static void query_topic_share(DList *L) {
    CLRSCR();
    puts("=== Запрос 2: доля книг по тематике ===");
    char topic[40];
    read_str("Тематика", topic, sizeof topic);

    long total = 0, match = 0;
    AuxList A; aux_init(&A);

    for (DNode *c = L->head; c; c = c->next) {
        total += c->data->copies;
        if (ICMP(c->data->topic, topic) == 0) {
            match += c->data->copies;
            aux_append(&A, c->data);
        }
    }

    printf("\nТематика    : «%s»\n", topic);
    if (!total) {
        puts("Библиотека пуста.");
        pause_enter();
    } else {
        printf("Найдено     : %zu назв.,  %ld экз.\n", A.count, match);
        printf("Всего в б-ке: %ld экз.\n", total);
        printf("Доля        : %.1f%%\n\n", 100.0 * match / total);
        if (A.count)
            paged_view(&A, "Книги по теме");
        else
            pause_enter();
    }
    aux_free(&A);
}


/* ═══════════════════════════════════════════════════════════
 *                      УДАЛЕНИЕ
 * ═══════════════════════════════════════════════════════════ */

static void remove_book(DList *L) {
    if (!L->count) { puts("Список пуст."); pause_enter(); return; }
    CLRSCR();
    puts("=== Удаление книги ===");
    puts("1. По фамилии автора");
    puts("2. По названию");
    int ch = read_int("Выбор");
    char val[80];
    int (*pred)(const Book *, const void *) = NULL;
    if      (ch == 1) { read_str("Фамилия автора", val, sizeof val); pred = pred_author; }
    else if (ch == 2) { read_str("Название",       val, sizeof val); pred = pred_title;  }
    else { puts("Отмена."); pause_enter(); return; }

    if (dlist_remove(L, pred, val))
        puts("Книга удалена.");
    else
        puts("Запись не найдена.");
    pause_enter();
}


/* ═══════════════════════════════════════════════════════════
 *            ДВУХУРОВНЕВОЕ МЕНЮ
 * ═══════════════════════════════════════════════════════════ */

/* Подменю — Данные */
static void menu_data(DList *L) {
    for (;;) {
        CLRSCR();
        printf("=== ДАННЫЕ  [книг: %zu] ===\n", L->count);
        puts("  1. Добавить книгу (в конец списка)");
        puts("  2. Добавить книгу (в упорядоченный по автору)");
        puts("  3. Удалить книгу");
        puts("  4. Просмотр (вперёд)");
        puts("  5. Просмотр (назад)");
        puts("  0. Назад");
        switch (read_int("Выбор")) {
            case 1: {
                Book *b = input_book();
                if (b) { dlist_append(L, b); puts("Добавлено."); pause_enter(); }
                break;
            }
            case 2: {
                Book *b = input_book();
                if (b) {
                    dlist_insert_sorted(L, b, cmp_author);
                    puts("Добавлено в упорядоченный список."); pause_enter();
                }
                break;
            }
            case 3: remove_book(L);   break;
            case 4: view_forward(L);  break;
            case 5: view_backward(L); break;
            case 0: return;
        }
    }
}

/* Подменю — Поиск и сортировка */
static void menu_search(DList *L) {
    for (;;) {
        CLRSCR();
        puts("=== ПОИСК И СОРТИРОВКА ===");
        puts("  1. Сортировка по автору (А→Я)");
        puts("  2. Сортировка по названию");
        puts("  3. Сортировка по году (новые→старые)");
        puts("  4. Сортировка по году (старые→новые)");
        puts("  5. Сортировка по издательству");
        puts("  6. Сортировка по тематике");
        puts("  7. Сортировка по кол-ву экземпляров");
        puts("  8. [Запрос 1] Книги издательства за последние 5 лет");
        puts("  9. [Запрос 2] Доля книг по заданной тематике");
        puts("  0. Назад");
        switch (read_int("Выбор")) {
            case 1: show_sorted(L, cmp_author,    "Сортировка по автору");             break;
            case 2: show_sorted(L, cmp_title,     "Сортировка по названию");           break;
            case 3: show_sorted(L, cmp_year_desc, "Сортировка по году (нов→стар)");    break;
            case 4: show_sorted(L, cmp_year_asc,  "Сортировка по году (стар→нов)");    break;
            case 5: show_sorted(L, cmp_publisher, "Сортировка по издательству");       break;
            case 6: show_sorted(L, cmp_topic,     "Сортировка по тематике");           break;
            case 7: show_sorted(L, cmp_copies,    "Сортировка по кол-ву экземпляров"); break;
            case 8: query_publisher_5years(L); break;
            case 9: query_topic_share(L);      break;
            case 0: return;
        }
    }
}


/* ═══════════════════════════════════════════════════════════
 *                         MAIN
 * ═══════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    /* Имя файла: argv[1] → с клавиатуры → DEFAULT_FILE */
    char fname[256];
    if (argc > 1) {
        strncpy(fname, argv[1], sizeof fname - 1);
        fname[sizeof fname - 1] = '\0';
    } else {
        printf("Имя файла данных [%s]: ", DEFAULT_FILE);
        fflush(stdout);
        fgets(fname, sizeof fname, stdin);
        char *p = strchr(fname, '\n');
        if (p) *p = '\0';
        if (fname[0] == '\0')
            strncpy(fname, DEFAULT_FILE, sizeof fname - 1);
    }

    DList lib;
    dlist_init(&lib);
    load_file(&lib, fname);
    pause_enter();

    /* Главное меню */
    for (;;) {
        CLRSCR();
        printf("=== БИБЛИОТЕКА  [файл: %s | книг: %zu] ===\n", fname, lib.count);
        puts("  1. Данные (добавить / удалить / просмотр)");
        puts("  2. Поиск и сортировка");
        puts("  0. Выход (с сохранением)");
        int ch = read_int("Выбор");
        if      (ch == 1) menu_data(&lib);
        else if (ch == 2) menu_search(&lib);
        else if (ch == 0) break;
    }

    save_file(&lib, fname);
    dlist_free(&lib);
    printf("До свидания!\n");
    return 0;
}
