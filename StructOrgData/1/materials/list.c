/*односвязный линейный список*/

#include <stdio.h>
#include <stdlib.h>

typedef int DataType;

void print (DataType x)
{
     printf ("%d ", x);
}

typedef
struct element
{
   DataType data;
   struct element * next;
} node;

node * new_node (node * pnode, DataType x) /*создание нового узла*/
{
       node * temp;
       temp = (node *) malloc (sizeof (node));
       temp->data = x;
       temp->next = pnode;
       return temp;
}

void del_list (node * head) /*удаление всего списка*/
{
     node * temp;
     while (head) /*пока в списке есть элементы*/
     {
           temp = head;
           head = head->next; /*переставляем указатель на следующий элемент*/
           free (temp); /*первый удаляем*/
     }
}

node * delete_node (node * pnode) /*удаление первого элемента*/
{
     node * temp;
     if (pnode) /*если список не пуст*/
     {
           temp = pnode;
           pnode = pnode->next; /*переставляем указатель на следующий элемент*/
           free (temp); /*первый удаляем*/
     }
     return pnode;
}

node * find_node (node * cur, DataType x) /*поиск элемента со значением х*/
{
     while (cur && cur->data != x) /*пока в списке есть элементы и они не равны х*/
           cur = cur->next; /*переставляем указатель на следующий элемент*/
     return cur;
}

node * find_last (node * cur) /*поиск последнего элемента списка*/
{
    if (cur == NULL)
        return NULL;
    while (cur->next)   /*пока есть еще элементы*/
        cur = cur->next;    /*переставляем указатель на следующий элемент*/
    return cur;
}

/*поиск элемента списка, предшествующего элементу со значением, большим или равным х.
Если такого нет, то возвращается адрес последнего элемента списка*/
node * find_prev (node * cur, DataType x)
{
    while (cur->next && cur->next->data < x) /*пока список не закончился и нужный элемент не найден*/
        cur = cur->next;    /*переходим к следующему*/
    return cur; /*возвращаем адрес предшествующего элемента*/
}

node * insert_node (node * head, DataType x)   /*вставка с учетом упорядоченности по возрастанию*/
{
    node * cur;
    if (head == NULL)     /*если список пуст*/
        return new_node(NULL, x);
    if (head->data > x)   /*если элемент меньше наименьшего в списке*/
        return new_node(head, x);   /*добавляем его в голову списка*/
    cur = find_prev(head, x);   /*поиск места для вставки*/
    cur->next = new_node(cur->next, x); /*вставка на найденную позицию*/
    return head;
}

void print_list (node * pnode) /*печать списка*/
{
     if (!pnode) /*если список пуст*/
     {
           printf ("Список пуст\n");
           return;
     }
     while (pnode) /*пока в списке есть элементы*/
     {
           print (pnode->data);
           pnode = pnode->next; /*переставляем указатель на следующий элемент*/
     }
}

void print_list_recursive (node * head) /*рекурсивная функция печати списка*/
{
     if (head) /*если есть что выводить*/
     {
        print (head->data); /*выводим текущее значение*/
        print_list_recursive (head->next); /*печатаем список, начинающийся со следующего элемента*/
     }
}

void print_list_recursive_back (node * head) /*рекурсивная функция печати списка в обратном порядке*/
{
     if (head) /*если есть список*/
     {
        print_list_recursive_back (head->next); /*печатаем список, начинающийся со следующего элемента*/
        print (head->data); /*печатаем текущее значение*/
     }
}

int main(int argc, char *argv[])
{
    struct element * head = NULL, *tmp;
    int a, i;
    printf ("Введите 5 чисел\n");
    for (i=0; i<5; i++)
    {
        scanf ("%d", &a);
        head = insert_node(head, a);
    }
    print_list (head);
    printf ("\n");
    tmp = find_last(head);
    printf ("Последний элемент списка ");
    print (tmp->data);
    printf ("\n");
    print_list (head);
    printf ("\n");
    head = delete_node (head); /*удаляем первый элемент*/
    print_list (head);
    printf ("\n");
    printf ("Введите число для поиска\n");
    scanf ("%d", &a);
    tmp = find_node(head, a);
    if (tmp)
    {
        printf ("Элемент найден");
        tmp=find_prev(head, a);
        tmp->next = delete_node(tmp->next);/*удаляем найденный элемент*/
        printf (" и удален\n");
    }
    else
        printf ("Такого элемента в списке нет\n");
    print_list (head);
    printf ("\nРекурсивной функцией:\n");
    print_list_recursive (head);
    printf ("\nВ обратном порядке:\n");
    print_list_recursive_back (head);
    printf ("\n");
    del_list (head);
    return 0;
}
