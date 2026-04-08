/*двусвязный линейный список*/

#include <stdio.h>
#include <stdlib.h>

typedef int DataType;

struct element
{
       DataType data;
       struct element * next, *prev;
};

struct List
{
       struct element * head, * tail;
};

void makenull (struct List *list)
{
     list->head = list->tail = NULL;
}

struct List add_head (struct List list, DataType x) /*добавление в начало*/
{
       struct element * temp;
       temp = (struct element *) malloc (sizeof (struct element));
       temp->data = x;
       temp->next = list.head;
       temp->prev = NULL;
       if (list.head) /*если добавляем в непустой список*/
          list.head->prev = temp;
       else /*если добавляемый элемент единственный в списке*/
          list.tail = temp;
       list.head = temp;
       return list;
}

struct List add_tail (struct List list, DataType x) /*добавление в конец*/
{
       struct element * temp;
       temp = (struct element *) malloc (sizeof (struct element));
       temp->data = x;
       temp->next = NULL;
       temp->prev = list.tail;
       if (list.tail) /*если добавляем в непустой список*/
          list.tail->next = temp;
       else /*если добавляемый элемент единственный в списке*/
          list.head = temp;
       list.tail = temp;
       return list;
}

void insert (struct element * cur, DataType x) /*вставка после текущего (не последнего!)*/
{
       struct element * temp;
       temp = (struct element *) malloc (sizeof (struct element));
       temp->data = x;
       temp->next = cur->next;
       temp->prev = cur;
       cur->next = temp;
       temp->next->prev = temp; /*указателю prev следующего элемента присваиваем адрес добавляемого*/
}

void del_list (struct List list) /*удаление списка*/
{
     struct element * temp;
     while (list.head) /*пока в списке есть элементы*/
     {
           temp = list.head;
           list.head = list.head->next; /*переставляем указатель на следующий элемент*/
           free (temp); /*первый удаляем*/
     }
}

struct List del_head (struct List list) /*удаление первого элемента*/
{
     struct element * temp;
     if (list.head) /*если в списке есть элементы*/
     {
           temp = list.head;
           list.head = list.head->next; /*переставляем указатель на следующий элемент*/
           if (list.head == NULL) /*если удаляемый элемент был единственным*/
               list.tail = NULL;
           else
               list.head->prev = NULL; /*обнуляем указатель на удаляемый элемент*/
           free (temp); /*первый удаляем*/
     }
     return list;
}

struct List del_tail (struct List list) /*удаление последнего элемента*/
{
     struct element * temp;
     if (list.tail) /*если в списке есть элементы*/
     {
           temp = list.tail;
           list.tail = list.tail->prev; /*переставляем указатель на предыдущий элемент*/
           if (list.tail == NULL) /*если удаляемый элемент был единственным*/
                list.head = NULL;
           else
                list.tail->next = NULL; /*обнуляем указатель на удаляемый элемент*/
           free (temp); /*последний удаляем*/
     }
     return list;
}

void del_element (struct element * cur) /*удаление текущего (важно(!) не первого и не последнего)*/
{
       cur->next->prev = cur->prev; /*указателю prev следующего элемента присваиваем адрес предшествующего удаляемому*/
       cur->prev->next = cur->next; /*указателю next предыдущего элемента присваиваем адрес следующего за удаляемым*/
       free (cur);
}

void print_list (struct List list) /*печать списка*/
{
     while (list.head) /*пока в списке есть элементы*/
     {
           printf ("%d ", list.head->data);
           list.head = list.head->next; /*переставляем указатель на следующий элемент*/
     }
}

void print_list_back (struct List list) /*печать списка в обратном порядке*/
{
     while (list.tail) /*пока в списке есть элементы*/
     {
           printf ("%d ", list.tail->data);
           list.tail = list.tail->prev; /*переставляем указатель на следующий элемент*/
     }
}

int main(int argc, char *argv[])
{
  struct List list;
  int x;

  makenull (&list);
  for (x=1; x<5; x++)
      list = add_head (list, x);
  print_list (list);
  printf ("\n");
  for (x=10; x<15; x++)
      list = add_tail (list, x);
  print_list (list);
  printf ("\n");
  print_list_back (list);
  printf ("\n");
  insert (list.head->next, 1000); /*вставка числа 1000 после второго элемента списка*/
  print_list (list);
  printf ("\n");
  list = del_head(list); /*удаление первого элемента*/
  print_list (list);
  printf ("\n");
  list = del_tail (list); /*удаление последнего элемента*/
  print_list (list);
  printf ("\n");
  del_element (list.tail->prev); /*удаление предпоследнего элемента*/
  print_list (list);
  printf ("\n");
  del_list (list);
  system("PAUSE");
  return 0;
}
