
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/hashtable.h>
#include <linux/mutex.h>

#include "rpncalc.h"

struct rpncalc_entry {
	double value;						// The value of this entry.
	struct list_head next;				// Linked list pointers for stack.
};

struct rpncalc {
	int handle;							// Assigned handle for this rpncalc.
	struct mutex lock;					// Calculator lock.
	struct list_head stack;				// The stack for this calculator.
	int size;							// The size of the stack.
	struct hlist_node next;				// Hash list pointers for calculator hashtable.
};

DEFINE_HASHTABLE(calcs, 3);				// Declare the calculators hashtable.
DEFINE_MUTEX(calcs_lock);				// Declare calculators hashtable lock.

static unsigned int next_handle = 0;	// Keeps track of the next available calculator handle.

static struct rpncalc* get_rpncalc(int handle);
static struct rpncalc_entry* new_entry(void);
static int push(struct rpncalc* calc, struct rpncalc_entry* entry);
static int pop(struct rpncalc* calc, struct rpncalc_entry** entryp);
static int do_add(struct rpncalc* calc);
static int do_substract(struct rpncalc* calc);
static int do_multiply(struct rpncalc* calc);
static int do_divide(struct rpncalc* calc);

/**
 *	rpncalc_new - Allocate a new calculator.
 *  @handlep: pointer to return calculator handle with
 */
int	rpncalc_new(int* handlep) {
	struct rpncalc *calc;

	// Make sure handlep is valid;
	if(!handlep) {
		return RPNCALC_E_INVALID;
	}

	// Allocate memory for calculator.
	calc = kmalloc(sizeof(struct rpncalc), GFP_KERNEL);
	if(!calc) {
		return RPNCALC_E_NOMEM;
	}

	// Initialize the calculator.
	calc->handle = next_handle++;
	INIT_LIST_HEAD(&calc->stack);
	mutex_init(&calc->lock);

	// Lock the calculator table.
	mutex_lock(&calcs_lock);

	// Insert calculator into table.
	hash_add(calcs, &calc->next, calc->handle);

	// Assign calculator handle to return pointer.
	*handlep = calc->handle;

	// Unlock the calculator table.
	mutex_unlock(&calcs_lock);

	// Return success.
	return RPNCALC_E_SUCCESS;
}

/**
 *	rpncalc_delete - Free a calculator.
 *  @handle: handle of calculator
 */
int rpncalc_delete(int handle) {
	struct rpncalc* calc;
	struct rpncalc_entry* entry;

	// Lock the calculator table.
	mutex_lock(&calcs_lock);

	// Lookup calculator.
	calc = get_rpncalc(handle);
	if(!calc) {
		mutex_unlock(&calcs_lock);
		return RPNCALC_E_INVALID;
	}

	// Remove from table;
	hash_del(&calc->next);

	// Unlock the calculator table.
	mutex_unlock(&calcs_lock);

	// Lock the calculator.
	mutex_lock(&calc->lock);

	// Free the stack entries.
	while(!list_empty(&calc->stack)) {
		entry = list_first_entry(&calc->stack, struct rpncalc_entry, next);
		list_del(&entry->next);
		kfree(entry);
	}

	// Free the rpncalc.
	kfree(calc);

	return RPNCALC_E_SUCCESS;
}

/**
 *	rpncalc_push - Push a value onto the calculator stack.
 *	@handle - handle of calculator
 *	@value - value to push
 */
int rpncalc_push(int handle, double value) {
	struct rpncalc* calc;
	struct rpncalc_entry* entry;
	int retval;

	// Lock the calculator table.
	mutex_lock(&calcs_lock);
	
	// Lookup calculator.
	calc = get_rpncalc(handle);
	if(!calc) {
		mutex_unlock(&calcs_lock);
		return RPNCALC_E_INVALID;
	}

	// Unlock the calculator table.
	mutex_unlock(&calcs_lock);

	// Create a new entry.
	entry = new_entry();
	if(!entry) {
		return RPNCALC_E_NOMEM;
	}

	// Set value in entry.
	entry->value = value;

	// Lock the calculator.
	mutex_lock(&calc->lock);

	// Push the entry onto the stack.
	retval = push(calc, entry);

	// Unlock the calculator.
	mutex_unlock(&calc->lock);

	return retval;
}

/**
 *	rpncalc_pop - Pop a value off the calculator stack.
 *	@handle - handle of calculator
 *	@valuep - optional pointer to return value with
 */
int rpncalc_pop(int handle, double* valuep) {
	struct rpncalc* calc;
	struct rpncalc_entry* entry;
	int retval;

	// Lock the calculator table.
	mutex_lock(&calcs_lock);

	// Look up calculator.
	calc = get_rpncalc(handle);
	if(!calc) {
		mutex_unlock(&calcs_lock);
		return RPNCALC_E_INVALID;
	}

	// Unlock the calculator table.
	mutex_unlock(&calcs_lock);

	// Lock the calculator.
	mutex_lock(&calc->lock);

	// Pop the calculator stack.
	retval = pop(calc, &entry);
	if(retval != RPNCALC_E_SUCCESS) {
		mutex_unlock(&calc->lock);
		return retval;
	}

	// Unlock the calculator.
	mutex_unlock(&calc->lock);

	// If valuep is valid, assign the entry value to return;
	if(valuep) {
		*valuep = entry->value;
	}

	// Free the entry memory.
	kfree(entry);

	return RPNCALC_E_SUCCESS;
}

/**
 *	rpncalc_op - Perform mathematical operation on calculator stack.
 *	@handle - handle of calculator
 *	@valuep - optional pointer to return value with
 */
int rpncalc_op(int handle, char op, double* valuep) {
	struct rpncalc* calc;
	struct rpncalc_entry* entry;
	int retval;

	// Lock the calculator table.
	mutex_lock(&calcs_lock);

	// Look up calculator.
	calc = get_rpncalc(handle);
	if(!calc) {
		mutex_unlock(&calcs_lock);
		return RPNCALC_E_INVALID;
	}

	// Unlock the calculator table.
	mutex_unlock(&calcs_lock);

	// Lock the calculator.
	mutex_lock(&calc->lock);

	// Perform the operation.
	switch(op) {
		case '+':
		{
			retval = do_add(calc);
			break;
		}
		case '-':
		{
			retval = do_substract(calc);
			break;
		}
		case '*':
		{
			retval = do_multiply(calc);
			break;
		}
		case '/':
		{
			retval = do_divide(calc);
			break;
		}
		default:
		{
			return RPNCALC_E_INVALID;
		}
	}

	// If operation was not successful, return.
	if(retval != RPNCALC_E_SUCCESS) {
		mutex_unlock(&calc->lock);
		return retval;
	}

	// If valuep is valid, get the top of the stack and return it.
	if(valuep) {
		entry = list_first_entry(&calc->stack, struct rpncalc_entry, next);
		*valuep = entry->value;
	}

	// Unlock the calculator.
	mutex_unlock(&calc->lock);

	return RPNCALC_E_SUCCESS;
}

/**
 *	rpncalc_size - Return size of calculator stack.
 *	@handle - handle of calculator
 *	@sizep - pointer to return size of stack with
 */
int rpncalc_size(int handle, int* sizep) {
	struct rpncalc* calc;

	// Make sure sizep is valid.
	if(!sizep) {
		return RPNCALC_E_INVALID;
	}

	// Lock the calculator table.
	mutex_lock(&calcs_lock);

	// Look up calculator.
	calc = get_rpncalc(handle);
	if(!calc) {
		mutex_unlock(&calcs_lock);
		return RPNCALC_E_INVALID;
	}

	// Unlock the calculator table.
	mutex_unlock(&calcs_lock);

	// Lock the calculator.
	mutex_lock(&calc->lock);

	// Set the return value.
	*sizep = calc->size;

	// Unlock the calculator.
	mutex_unlock(&calc->lock);

	return RPNCALC_E_SUCCESS;
}

/**
 *	rpncalc - Return value at particular index of stack.
 *	@handle - handle of calculator
 *	@index - index to return
 *	@valuep - pointer to return value with
 */
int rpncalc_at(int handle, int index, double* valuep) {
	struct rpncalc* calc;
	struct rpncalc_entry* entry;
	int i;

	// Make sure sizep is valid.
	if(!valuep) {
		return RPNCALC_E_INVALID;
	}

	// Lock the calculator table.
	mutex_lock(&calcs_lock);

	// Look up calculator.
	calc = get_rpncalc(handle);
	if(!calc) {
		return RPNCALC_E_INVALID;
	}

	// Unlock the calculator table.
	mutex_unlock(&calcs_lock);

	// Make sure index is valid.
	if(index < 0 || index >= calc->size) {
		return RPNCALC_E_INVALID;
	}

	// Lock the calculator.
	mutex_lock(&calc->lock);

	// Iterate until we get to the index element.
	entry = list_first_entry(&calc->stack, struct rpncalc_entry, next);
	for(i = 0; i < index; i++) {
		entry = list_next_entry(entry, next);
	}

	// Get the value entry.
	*valuep = entry->value;

	// Unlock the calculator.
	mutex_unlock(&calc->lock);

	return RPNCALC_E_SUCCESS;
}

static struct rpncalc* get_rpncalc(int handle) {
	struct rpncalc* calc = 0;
	struct rpncalc* cur;

	// Iterator over all rpncalcs in bucket mapped to by handle looking
	// for rpncalc with handle we are looking for.
	hash_for_each_possible(calcs, cur, next, handle){
		if(cur->handle == handle) {
			calc = cur;
			break;
		}
	}

	return calc;
}

static struct rpncalc_entry* new_entry() {
	struct rpncalc_entry* entry;

	// Allocate and initialize memory for entry.
	entry = kmalloc(sizeof(struct rpncalc_entry), GFP_KERNEL);
	if(entry) {
		INIT_LIST_HEAD(&entry->next);
	}
	
	return entry;
}

static int push(struct rpncalc* calc, struct rpncalc_entry* entry) {

	// Fail if entry is not valid.
	if(!entry) {
		return RPNCALC_E_INVALID;
	}

	// Add the entry to the stack and increment size.
	list_add(&entry->next, &calc->stack);
	calc->size++;

	return RPNCALC_E_SUCCESS;
}

static int pop(struct rpncalc* calc, struct rpncalc_entry** entryp) {

	// Fail if the stack is empty.
	if(calc->size == 0) {
		return RPNCALC_E_INSUFFICIENT;
	}

	// Fail if return pointer is invalid.
	if(!entryp) {
		return RPNCALC_E_INVALID;
	}

	// Get the first entry in the stack, delete it, and decrement size.
	*entryp = list_first_entry(&calc->stack, struct rpncalc_entry, next);
	list_del(&(*entryp)->next);
	calc->size--;

	return RPNCALC_E_SUCCESS;
}

static int do_add(struct rpncalc* calc) {
	struct rpncalc_entry* op1;
	struct rpncalc_entry* op2;
	struct rpncalc_entry* result;
	int retval;

	// Check that there are at least two entries on the stack.
	if(calc->size < 2) {
		return RPNCALC_E_INSUFFICIENT;
	}

	// Create a new entry for the result.
	result = new_entry();
	if(!result) {
		return RPNCALC_E_NOMEM;
	}

	// Pop first operand off stack.
	retval = pop(calc, &op1);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Pop second operand off stack.
	retval = pop(calc, &op2);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Add the two operands.
	result->value = op2->value + op1->value;

	// Push the result back on the stack and return.
	return push(calc, result);
}

static int do_substract(struct rpncalc* calc) {
	struct rpncalc_entry* op1;
	struct rpncalc_entry* op2;
	struct rpncalc_entry* result;
	int retval;

	// Check that there are at least two entries on the stack.
	if(calc->size < 2) {
		return RPNCALC_E_INSUFFICIENT;
	}

	// Create a new entry for the result.
	result = new_entry();
	if(!result) {
		return RPNCALC_E_NOMEM;
	}

	// Pop first operand off stack.
	retval = pop(calc, &op1);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Pop second operand off stack.
	retval = pop(calc, &op2);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Subtract the two operands.
	result->value = op2->value - op1->value;

	// Push the result back on the stack and return.
	return push(calc, result);
}

static int do_multiply(struct rpncalc* calc) {
	struct rpncalc_entry* op1;
	struct rpncalc_entry* op2;
	struct rpncalc_entry* result;
	int retval;

	// Check that there are at least two entries on the stack.
	if(calc->size < 2) {
		return RPNCALC_E_INSUFFICIENT;
	}

	// Create a new entry for the result.
	result = new_entry();
	if(!result) {
		return RPNCALC_E_NOMEM;
	}

	// Pop first operand off stack.
	retval = pop(calc, &op1);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Pop second operand off stack.
	retval = pop(calc, &op2);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Multiply the two operands.
	result->value = op2->value * op1->value;

	// Push the result back on the stack and return.
	return push(calc, result);
}

static int do_divide(struct rpncalc* calc) {
	struct rpncalc_entry* op1;
	struct rpncalc_entry* op2;
	struct rpncalc_entry* result;
	int retval;

	// Check that there are at least two entries on the stack.
	if(calc->size < 2) {
		return RPNCALC_E_INSUFFICIENT;
	}

	// Create a new entry for the result.
	result = new_entry();
	if(!result) {
		return RPNCALC_E_NOMEM;
	}

	// Pop first operand off stack.
	retval = pop(calc, &op1);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Pop second operand off stack.
	retval = pop(calc, &op2);
	if(retval != RPNCALC_E_SUCCESS) {
		return retval;
	}

	// Divide the two operands.
	result->value = op2->value / op1->value;

	// Push the result back on the stack and return.
	return push(calc, result);
}
