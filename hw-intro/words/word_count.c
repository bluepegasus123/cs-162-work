/*

Copyright © 2019 University of California, Berkeley

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

word_count provides lists of words and associated count

Functional methods take the head of a list as first arg.
Mutators take a reference to a list as first arg.
*/

#include "word_count.h"

/* Basic utilities */

char *new_string(char *str) {
  char *new_str = (char *) malloc(strlen(str) + 1);
  if (new_str == NULL) {
    return NULL;
  }
  return strcpy(new_str, str);
}

int init_words(WordCount **wclist) {
  /* Initialize word count.
     Returns 0 if no errors are encountered
     in the body of this function; 1 otherwise.
  */
  // Want to store an address in *wclist
  // *wclist is dereferencing the first pointer
  
  *wclist = NULL;
  return 0;
}

ssize_t len_words(WordCount *wchead) {
  /* Return -1 if any errors are
     encountered in the body of
     this function.
  */
  if (wchead == NULL) {
    return -1;
  }
    size_t len = 0;
    while (wchead != NULL) {
      len++;
      wchead = wchead->next;
    }
    return len;
}

WordCount *find_word(WordCount *wchead, char *word) {
  /* Return count for word, if it exists */
  if (wchead == NULL || word == NULL) {
    return NULL;
  }
  WordCount *wc = wchead;
  while (wc != NULL && strcmp(wc->word, word) != 0) {
    wc = wc->next;
  }
  return wc;
}

int add_word(WordCount **wclist, char *word) {
  // printf("%p\n", word);
  // printf("%p\n", *wclist);
  if (wclist == NULL || word == NULL) {
    return 1;
  }
  // if (*wclist == NULL) {
  //   return 1;
  // }
  
  // printf("%p\n", *wclist);
  /* If word is present in word_counts list, increment the count.
     Otherwise insert with count 1.
     Returns 0 if no errors are encountered in the body of this function; 1 otherwise.
  */
  WordCount *head = *wclist;
  WordCount *foundNode = find_word(head, word);
  if (foundNode == NULL) {
    //allocate here
    // if main.c will sort this list for me, then i can insert at the end of the list
    // insert to beginning of the list to make it easier and it'll be corrected later on
    WordCount *allocatedNode = malloc(sizeof(WordCount));
    if (allocatedNode == NULL) {
      return 1;
    }
    allocatedNode->count = 1;
    char* newStringPointer = new_string(word);
    if (newStringPointer == NULL) {
      return 1;
    }
    allocatedNode->word = newStringPointer;

    // set the current head to be this
    *wclist = allocatedNode;
    allocatedNode->next = head;
  } else {
    // adjust the foundNode's count property
    foundNode->count = foundNode->count + 1;
  }
  return 0;

  
}

void fprint_words(WordCount *wchead, FILE *ofile) {
  /* print word counts to a file */
  WordCount *wc;
  for (wc = wchead; wc; wc = wc->next) {
    fprintf(ofile, "%i\t%s\n", wc->count, wc->word);
  }
}
