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
#  include <direct.h>
#  define CLRSCR()      system("cls")
#  define ICMP(a, b)    _stricmp((a), (b))
#  define MKDIR_DATA()  _mkdir("data")
#else
#  include <strings.h>
#  include <sys/stat.h>
#  define CLRSCR()      system("clear")
#  define ICMP(a, b)    strcasecmp((a), (b))
#  define MKDIR_DATA()  mkdir("data", 0755)
#endif

#define PAGE_SIZE    5
#define DEFAULT_FILE "data/library.dat"

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

/* Нормализует имя файла, введённое с клавиатуры (без явного пути):
 *   1) нет расширения → дописывает ".dat"  ("mylib"   → "mylib.dat")
 *   2) нет слеша     → вставляет "data/"   ("mylib.dat" → "data/mylib.dat")
 * Если в имени уже есть '/' или '\' — не трогает совсем. */
static void normalize_fname(char *fname, size_t maxlen) {
    if (strchr(fname, '/') != NULL || strchr(fname, '\\') != NULL)
        return;                        /* явный путь — оставляем как есть */
    if (strchr(fname, '.') == NULL) {  /* нет точки — дописываем .dat */
        size_t len = strlen(fname);
        if (len + 4 < maxlen)
            strcat(fname, ".dat");
    }
    size_t len = strlen(fname);        /* вставляем "data/" в начало */
    if (len + 5 < maxlen) {
        memmove(fname + 5, fname, len + 1);
        memcpy(fname, "data/", 5);
    }
}

/* Создаём папку data/ если её нет (ошибка EEXIST — норма, игнорируем). */
static void ensure_data_dir(void) {
    MKDIR_DATA();
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

static int cmp_author   (const Book *a, const Book *b) { return ICMP(a->author,    b->author);    }
static int cmp_title    (const Book *a, const Book *b) { return ICMP(a->title,     b->title);     }
static int cmp_year_asc (const Book *a, const Book *b) { return a->year    - b->year;              }
static int cmp_year_desc(const Book *a, const Book *b) { return b->year    - a->year;              }
static int cmp_publisher(const Book *a, const Book *b) { return ICMP(a->publisher, b->publisher); }
static int cmp_topic    (const Book *a, const Book *b) { return ICMP(a->topic,     b->topic);     }
static int cmp_copies   (const Book *a, const Book *b) { return a->copies  - b->copies;            }

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

/* Совпадение по адресу данных — для удаления записи, выбранной из списка */
static int pred_ptr(const Book *b, const void *arg) {
    return b == (const Book *)arg;
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
 *  ВЫБОР ИЗ УНИКАЛЬНЫХ ЗНАЧЕНИЙ ПОЛЯ
 *  get  — функция, возвращающая нужное поле книги (publisher / topic / …)
 *  Показывает пронумерованный список уникальных значений (отсортирован),
 *  пункт 0 — ввести вручную. Возвращает 1 при выборе, 0 при отмене.
 * ═══════════════════════════════════════════════════════════ */

static const char *book_publisher(const Book *b) { return b->publisher; }
static const char *book_topic    (const Book *b) { return b->topic;     }

static int pick_unique(DList *L,
        const char *(*get)(const Book *),
        const char *label, char *out, size_t outlen)
{
    char vals[64][80];
    int  n = 0;

    /* собираем уникальные значения */
    for (DNode *c = L->head; c && n < 64; c = c->next) {
        const char *v = get(c->data);
        int dup = 0;
        for (int i = 0; i < n; i++)
            if (ICMP(vals[i], v) == 0) { dup = 1; break; }
        if (!dup) {
            strncpy(vals[n], v, sizeof vals[n] - 1);
            vals[n][sizeof vals[n] - 1] = '\0';
            n++;
        }
    }
    if (!n) { puts("Список пуст."); return 0; }

    /* сортируем (список маленький — пузырёк достаточен) */
    for (int i = 0; i < n-1; i++)
        for (int j = i+1; j < n; j++)
            if (ICMP(vals[i], vals[j]) > 0) {
                char tmp[80];
                strncpy(tmp,    vals[i], sizeof tmp    - 1);
                strncpy(vals[i], vals[j], sizeof vals[i] - 1);
                strncpy(vals[j], tmp,    sizeof vals[j] - 1);
            }

    printf("\n%s:\n", label);
    for (int i = 0; i < n; i++)
        printf("  %2d. %s\n", i+1, vals[i]);
    puts("   0. Ввести вручную");

    int ch = read_int("Выбор");
    if (ch >= 1 && ch <= n) {
        strncpy(out, vals[ch-1], outlen - 1);
        out[outlen - 1] = '\0';
        return 1;
    }
    if (ch == 0) {
        read_str(label, out, (int)outlen);
        return out[0] != '\0';
    }
    return 0;
}


/* ═══════════════════════════════════════════════════════════
 *               ЗАПРОСЫ ВАРИАНТА 10
 * ═══════════════════════════════════════════════════════════ */

/* Запрос 1: авторы и названия книг заданного издательства за последние 5 лет */
static void query_publisher_5years(DList *L) {
    CLRSCR();
    puts("=== Запрос 1: книги издательства за последние 5 лет ===");
    char pub[50];
    if (!pick_unique(L, book_publisher, "Издательство", pub, sizeof pub))
        return;

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
    if (!pick_unique(L, book_topic, "Тематика", topic, sizeof topic))
        return;

    long total = 0, match = 0;
    AuxList A; aux_init(&A);

    for (DNode *c = L->head; c; c = c->next) {
        total += c->data->copies;
        if (ICMP(c->data->topic, topic) == 0) {
            match += c->data->copies;
            aux_append(&A, c->data);
        }
    }

    if (!total) {
        puts("Библиотека пуста.");
        pause_enter();
        aux_free(&A);
        return;
    }

    /* Заголовок содержит тему и долю — статистика видна прямо в шапке
       пагинатора и не стирается его CLRSCR().                          */
    char title[160];
    snprintf(title, sizeof title,
             "Книги по теме «%s»: %ld из %ld экз. (%.1f%%)",
             topic, match, total, 100.0 * match / total);

    if (A.count) {
        paged_view(&A, title);
    } else {
        CLRSCR();
        printf("=== %s ===\n", title);
        puts("Книг с такой тематикой не найдено.");
        pause_enter();
    }
    aux_free(&A);
}


/* ═══════════════════════════════════════════════════════════
 *              ДЕМОНСТРАЦИОННЫЕ ДАННЫЕ  (20 книг)
 *
 * Подобраны так, чтобы показать оба запроса варианта 10:
 *   Запрос 1 — издательство «Питер», 2021–2026:
 *             Страуструп, Кормен, Шилдт, Лутц, Прата,
 *             Таненбаум, Ван Россум  (7 книг)
 *   Запрос 2 — тематика «Программирование»:
 *             Страуструп(150) + Шилдт(180) + Лутц(120) +
 *             Прата(95) + Мартин(160) + Фаулер(70) +
 *             Ван Россум(50)  = 825 экз. из 3440 итого ≈ 24%
 * ═══════════════════════════════════════════════════════════ */

static void add_demo_book(DList *L,
        const char *author, const char *title,
        const char *publisher, int year,
        const char *topic, int copies,
        BookKind kind, union BookExtra extra)
{
    Book *b = calloc(1, sizeof *b);
    if (!b) return;
    strncpy(b->author,    author,    sizeof b->author    - 1);
    strncpy(b->title,     title,     sizeof b->title     - 1);
    strncpy(b->publisher, publisher, sizeof b->publisher - 1);
    b->year   = year;
    strncpy(b->topic, topic, sizeof b->topic - 1);
    b->copies = copies;
    b->kind   = kind;
    b->extra  = extra;
    dlist_append(L, b);
}

static void load_demo_data(DList *L) {
    union BookExtra e;

    /* ── Учебники ─────────────────────────────────────────── */
#define TXT(disc, lvl) \
    (memset(&e, 0, sizeof e), \
     strncpy(e.txt.discipline, (disc), sizeof e.txt.discipline - 1), \
     e.txt.level = (lvl), e)

    add_demo_book(L, "Страуструп Б.",
        "Язык программирования C++",
        "Питер",         2023, "Программирование", 150, TEXTBOOK,  TXT("Информатика",      1));
    add_demo_book(L, "Кормен Т.",
        "Алгоритмы: построение и анализ",
        "Питер",         2022, "Алгоритмы",        200, TEXTBOOK,  TXT("Дискр. математика", 1));
    add_demo_book(L, "Шилдт Г.",
        "Java. Полное руководство",
        "Питер",         2021, "Программирование", 180, TEXTBOOK,  TXT("ООП",               1));
    add_demo_book(L, "Лутц М.",
        "Программирование на Python",
        "Питер",         2024, "Программирование", 120, TEXTBOOK,  TXT("Программирование",  1));
    add_demo_book(L, "Прата С.",
        "Язык программирования C",
        "Питер",         2025, "Программирование",  95, TEXTBOOK,  TXT("Информатика",       1));
    add_demo_book(L, "Таненбаум Э.",
        "Современные операционные системы",
        "Питер",         2021, "Системное ПО",     130, TEXTBOOK,  TXT("Операц. системы",   1));
    add_demo_book(L, "Дейт К.",
        "Введение в базы данных",
        "БХВ-Петербург", 2023, "Базы данных",      110, TEXTBOOK,  TXT("Базы данных",       1));
    add_demo_book(L, "Мартин Р.",
        "Чистый код",
        "БХВ-Петербург", 2024, "Программирование", 160, TEXTBOOK,  TXT("Разработка ПО",     1));
    add_demo_book(L, "Ван Россум Г.",
        "Документация Python 3",
        "Питер",         2025, "Программирование",  50, TEXTBOOK,  TXT("Python",             1));
#undef TXT

    /* ── Научные ──────────────────────────────────────────── */
#define SCI(u, src) \
    (memset(&e, 0, sizeof e), \
     strncpy(e.sci.udk, (u), sizeof e.sci.udk - 1), \
     e.sci.sources = (src), e)

    add_demo_book(L, "Кнут Д.",
        "Искусство программирования т.1",
        "Питер",         2019, "Алгоритмы",         80, SCIENTIFIC, SCI("004.421", 1500));
    add_demo_book(L, "Фаулер М.",
        "Рефакторинг",
        "БХВ-Петербург", 2020, "Программирование",  70, SCIENTIFIC, SCI("004.41",   250));
    add_demo_book(L, "Ландау Л.",
        "Теоретическая механика",
        "Наука",         1988, "Физика",             90, SCIENTIFIC, SCI("531",      450));
    add_demo_book(L, "Зельдович Я.",
        "Высшая математика для физиков",
        "Наука",         2002, "Математика",         55, SCIENTIFIC, SCI("517",      120));
    add_demo_book(L, "Колмогоров А.",
        "Теория вероятностей",
        "Наука",         1999, "Математика",         40, SCIENTIFIC, SCI("519.2",    180));
    add_demo_book(L, "Гейтс Б.",
        "Дорога в будущее",
        "Эксмо",         1995, "Информатика",        25, SCIENTIFIC, SCI("004",       30));
#undef SCI

    /* ── Художественные ───────────────────────────────────── */
#define FIC(g, s) \
    (memset(&e, 0, sizeof e), \
     strncpy(e.fic.genre,  (g), sizeof e.fic.genre  - 1), \
     strncpy(e.fic.series, (s), sizeof e.fic.series - 1), e)

    add_demo_book(L, "Толстой Л.Н.",
        "Война и мир",
        "АСТ",   1980, "Художественная", 500, FICTION, FIC("Роман-эпопея",   "Собрание сочинений"));
    add_demo_book(L, "Достоевский Ф.",
        "Преступление и наказание",
        "Эксмо", 1975, "Художественная", 400, FICTION, FIC("Роман",           "Классика"));
    add_demo_book(L, "Булгаков М.",
        "Мастер и Маргарита",
        "АСТ",   1990, "Художественная", 350, FICTION, FIC("Сатирич. роман",  "Золотая серия"));
    add_demo_book(L, "Стругацкий А.",
        "Пикник на обочине",
        "АСТ",   2000, "Фантастика",     280, FICTION, FIC("Фантастика",      "Миры Стругацких"));
    add_demo_book(L, "Лем С.",
        "Солярис",
        "Эксмо", 2022, "Фантастика",     160, FICTION, FIC("Фантастика",      "Мастера НФ"));
#undef FIC

    printf("Добавлено 20 демонстрационных книг.\n");
    pause_enter();
}


/* ═══════════════════════════════════════════════════════════
 *                      УДАЛЕНИЕ
 * ═══════════════════════════════════════════════════════════ */

/* Удаление с интегрированным постраничным просмотром.
   Пользователь листает список и, не выходя из него, вводит номер строки.
   После удаления список перерисовывается немедленно. */
static void remove_book(DList *L) {
    if (!L->count) { puts("Список пуст."); pause_enter(); return; }

    int page = 0;
    char line[32];

    for (;;) {
        int pages = (int)((L->count + PAGE_SIZE - 1) / PAGE_SIZE);
        if (page >= pages) page = pages - 1;

        CLRSCR();
        printf("=== Удаление  [стр. %d / %d,  всего: %zu] ===\n",
               page + 1, pages, L->count);
        print_table_header();

        /* Находим начало текущей страницы */
        DNode *cur = L->head;
        int start = page * PAGE_SIZE;
        for (int i = 0; i < start && cur; i++) cur = cur->next;

        DNode *page_start = cur;
        int row = start;
        for (int i = 0; i < PAGE_SIZE && cur; i++, cur = cur->next)
            print_book_row(++row, cur->data);
        puts(HLINE);

        cur = page_start; row = start;
        for (int i = 0; i < PAGE_SIZE && cur; i++, cur = cur->next)
            print_book_extra(++row, cur->data);

        /* Подсказка: можно писать "d3" или "d" (тогда запросит номер) */
        printf("\n[N]-Вперёд  [P]-Назад  [D<N>]-Удалить  [Q]-Выход > ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;

        char cmd = line[0];
        if      (cmd == 'q' || cmd == 'Q') break;
        else if (cmd == 'n' || cmd == 'N') { if (page < pages - 1) page++; }
        else if (cmd == 'p' || cmd == 'P') { if (page > 0) page--; }
        else if (cmd == 'd' || cmd == 'D') {
            /* Номер может быть вписан сразу ("d3") или введён отдельно */
            int n = 0;
            if (sscanf(line + 1, "%d", &n) != 1 ||
                    n <= 0 || (size_t)n > L->count) {
                printf("Номер строки (1–%zu), 0 — отмена: ", L->count);
                fflush(stdout);
                char nbuf[16];
                if (!fgets(nbuf, sizeof nbuf, stdin)) continue;
                if (sscanf(nbuf, "%d", &n) != 1 ||
                        n <= 0 || (size_t)n > L->count)
                    continue;
            }

            /* O(n) доход до нужного узла */
            DNode *t = L->head;
            for (int i = 1; i < n; i++) t = t->next;
            if (!t) continue;

            /* Подтверждение одной строкой — экран не очищаем */
            printf("Удалить #%d «", n);
            u8pad(t->data->author, 16);
            fputs(" — ", stdout);
            u8pad(t->data->title, 22);
            fputs("»? [1=Да / Enter=Нет]: ", stdout);
            fflush(stdout);
            char cbuf[8];
            if (!fgets(cbuf, sizeof cbuf, stdin)) continue;
            if (cbuf[0] != '1') continue;   /* любой ответ кроме "1" = отмена */

            /* Предикат pred_ptr передаётся параметром — требование задания */
            Book *target = t->data;
            if (dlist_remove(L, pred_ptr, target)) {
                int new_pages = (int)((L->count + PAGE_SIZE - 1) / PAGE_SIZE);
                if (L->count == 0) { puts("Список пуст."); pause_enter(); break; }
                if (page >= new_pages) page = new_pages - 1;
                /* Список перерисовывается на следующей итерации цикла */
            }
        }
    }
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
        puts("  6. Загрузить демо-данные (добавить 20 книг)");
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
            case 6: load_demo_data(L); break;
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

    ensure_data_dir();  /* создаём папку data/ если её ещё нет */

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
        if (fname[0] == '\0') {
            strncpy(fname, DEFAULT_FILE, sizeof fname - 1);
        } else {
            normalize_fname(fname, sizeof fname);
        }
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
