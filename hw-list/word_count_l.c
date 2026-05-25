/*
 * Implementation of the word_count interface using Pintos lists.
 *
 * You may modify this file, and are expected to modify it.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PINTOS_LIST
#error "PINTOS_LIST must be #define'd when compiling word_count_l.c"
#endif

#include "word_count.h"

char* new_string(char* str) {
  char *new_str = (char *) malloc(strlen(str) + 1);
  if (new_str == NULL) {
    return NULL;
  }
  return strcpy(new_str, str);
}

void init_words(word_count_list_t* wclist) { 
  /* TODO */
  // need to set 
  // wclist->head.prev = NULL;
  // wclist->head.next = &wclist->tail;
  // wclist->tail.prev = &wclist->head;
  // wclist->tail.next = NULL;
  // printf("GOES INTO INIT_WORDS\n");
  // printf("wclist %p - init_words\n", wclist);
  list_init(wclist);
  // printf("FINISHED WITH INIT_WORDS\n");
  struct list_elem *head = list_head(wclist);
  struct list_elem *tail = list_tail(wclist);

  // printf("list head: %p\n", head);
  // printf("list head prev: %p\n", head->prev);
  // printf("list head next: %p\n", head->next);
  // printf("list tail: %p\n", tail);
  // printf("list tail prev: %p\n", tail->prev);
  // printf("list tail next: %p\n", tail->next);
}

size_t len_words(word_count_list_t* wclist) {
  /* TODO */
  if (wclist == NULL) {
    return 0;
  }
  return list_size(wclist);
}

word_count_t* find_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  // printf("GOES INTO FIND_WORD\n");
  struct list_elem *start = list_head(wclist);
  start = start->next;
  while (start != NULL && start != list_tail(wclist)) {
    word_count_t *wrapper = list_entry(start, word_count_t, elem);
    // printf("wrapper string: %i\n", wrapper->count);
    // printf("word: %s\n", word);
    if (strcmp(wrapper->word, word) == 0) {
      return wrapper;
    }
    start = start->next;
  }
  return NULL;
}

word_count_t* add_word(word_count_list_t* wclist, char* word) {
  /* TODO */
  // use the abstractions given to you
  // printf("[add_word] wclist: %p\n", wclist);
  // printf("[add_word] word: %p\n", word);
  // if found, just increment the word_count_t struct's count member
  word_count_t *found = find_word(wclist, word);
  // printf("found list_elem: %p for word %s\n", found, word);
  if (found != NULL) {
      found->count = found->count + 1;
      return found;
  }
  // add word will insert a node in btwn the head + tail of list
  // if word DNE in list, first need to allocate memory for a new word_count_t struct - get * back
  // set the members accordingly
  // use list insert API with list_elem pointer
  word_count_t *allocatedPtr = (word_count_t*) malloc(sizeof(word_count_t));
  // printf("allocatedPtr: %p\n", allocatedPtr);
  // I will need to allocate memory for the string itself as the char* passed might be a stack variable
  if (allocatedPtr == NULL) {
    return NULL;
  }
  char *heapPointerForString = new_string(word);
  // printf("new string ptr: %p\n", heapPointerForString);
  if (heapPointerForString == NULL) {
    return NULL;
  }
  allocatedPtr->word = heapPointerForString;
  allocatedPtr->count = 1;
  // printf("new list_elem ptr %p\n", &(allocatedPtr->elem));
  // printf("list head ptr: %p\n", list_head(wclist));
  // list_elem member needs to be set accordingly
  list_push_back(wclist, &(allocatedPtr->elem));
  return allocatedPtr;
}

void fprint_words(word_count_list_t* wclist, FILE* outfile) {
  /* TODO */
  /* Please follow this format: fprintf(<file>, "%i\t%s\n", <count>, <word>); */
  struct list_elem *start = list_begin(wclist);
  while (start != NULL && start != list_tail(wclist)) {
    word_count_t *wrapper = list_entry(start, word_count_t, elem);
    // printf("wrapper->word %s\n", wrapper->word);
    fprintf(outfile, "%i\t%s\n", wrapper->count, wrapper->word);
    start = start->next;
  }
}

static bool less_list(const struct list_elem* ewc1, const struct list_elem* ewc2, void* aux) {
  /* TODO */
  bool (*less_func)(const word_count_t*, const word_count_t*) = aux;
  word_count_t *a = list_entry(ewc1, word_count_t, elem);
  word_count_t *b = list_entry(ewc2, word_count_t, elem);
  return less_func(a, b);
}

void wordcount_sort(word_count_list_t* wclist,
                    bool less(const word_count_t*, const word_count_t*)) {
  list_sort(wclist, less_list, less);
}


