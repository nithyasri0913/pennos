/* CS5480 Assignment 0
 * Author: Seungju Lee
 * Purpose: Creates a vector that can hold any pointer type.
 */

#include "./Vec.h"
#include <stdlib.h>
#include "./panic.h"

Vec vec_new(size_t initial_capacity, ptr_dtor_fn ele_dtor_fn) {
  // check if capacity is non-negative.
  if (initial_capacity < 0) {
    panic("vec_new: Capacity must be non-negative.\n");
  }

  Vec vec;
  vec.data = malloc(initial_capacity * sizeof(void*));
  if (vec.data == NULL) {
    panic("vec_new: data malloc failed\n");
  }
  vec.length = 0;
  vec.capacity = initial_capacity;
  vec.ele_dtor_fn = ele_dtor_fn;
  return vec;
}

ptr_t vec_get(Vec* self, size_t index) {
  // index bound check
  if (index < self->length && index >= 0) {
    return self->data[index];
  }
  panic("vec_get: index out of bound\n");
}

void vec_set(Vec* self, size_t index, ptr_t new_ele) {
  // index bound check
  if (index < self->length && index >= 0) {
    ptr_t* temp_data = self->data[index];
    // call destructor for original element
    if (self->ele_dtor_fn != NULL) {
      self->ele_dtor_fn(temp_data);
    }

    self->data[index] = new_ele;
  } else {
    panic("vec_set: index out of bound\n");
  }
}

void vec_push_back(Vec* self, ptr_t new_ele) {
  // check capacity and increase as needed
  if (self->capacity == self->length) {
    if (self->capacity == 0) {
      self->capacity = 1;
      vec_resize(self, self->capacity);
    } else {
      vec_resize(self, self->capacity * 2);
    }
  }

  self->data[self->length] = new_ele;
  self->length++;
}

bool vec_pop_back(Vec* self) {
  // length check
  if (self->length != 0) {
    // call destructor for the element
    if (self->ele_dtor_fn != NULL) {
      self->ele_dtor_fn(self->data[self->length - 1]);
    }
    self->length -= 1;
    return true;
  }
  return false;
}

void vec_insert(Vec* self, size_t index, ptr_t new_ele) {
  // check if the index is in bound
  if (index > self->length || index < 0) {
    panic("vec_insert: index out of bound\n");
  }
  // check capacity and increase as needed
  if (self->capacity == self->length) {
    if (self->capacity == 0) {
      self->capacity = 1;
      vec_resize(self, self->capacity);
    } else {
      vec_resize(self, self->capacity * 2);
    }
  }
  // making space for the element to insert
  for (size_t idx = self->length; idx > index; idx--) {
    self->data[idx] = self->data[idx - 1];
  }

  self->data[index] = new_ele;
  self->length++;
}

void vec_erase(Vec* self, size_t index) {
  // check if index is in bound
  if (index >= self->length || index < 0) {
    panic("vec_erase: index out of bound\n");
  }
  // call destructor for the element
  if (self->ele_dtor_fn != NULL) {
    if (self->data[index] != NULL) {
      self->ele_dtor_fn(self->data[index]);
    }
  }
  // move the elements forward and decrease length
  size_t idx;
  for (idx = index; idx < self->length - 1; idx++) {
    self->data[idx] = self->data[idx + 1];
  }
  self->length -= 1;
}

void vec_resize(Vec* self, size_t new_capacity) {
  // check the capacity
  if (new_capacity > self->length) {
    ptr_t* new_data = malloc(sizeof(void*) * new_capacity);
    if (new_data == NULL) {
      panic("vec_resize: data malloc failed\n");
    }
    // move the original data to new data
    for (size_t idx = 0; idx < self->length; idx++) {
      new_data[idx] = self->data[idx];
    }
    free(self->data);
    self->data = new_data;
    self->capacity = new_capacity;
  }
}

void vec_clear(Vec* self) {
  size_t idx;
  // call destructor and loop through element
  if (self->ele_dtor_fn != NULL) {
    for (idx = 0; idx < self->length; idx++) {
      self->ele_dtor_fn(self->data[idx]);
    }
  }
  self->length = 0;
}

void vec_destroy(Vec* self) {
  // clear the data and length
  vec_clear(self);
  // free other struct components
  if (self->data != NULL) {
    free(self->data);
  }
  self->capacity = 0;
  self->data = NULL;
  self->ele_dtor_fn = NULL;
}
