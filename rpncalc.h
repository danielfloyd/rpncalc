#ifndef _RPNCALC_H_
#define _RPNCALC_H_

#define RPNCALC_E_SUCCESS (0)
#define RPNCALC_E_NOMEM (-1)
#define RPNCALC_E_INVALID (-2)
#define RPNCALC_E_INSUFFICIENT (-3)

int	rpncalc_new(int* handlep);

int rpncalc_delete(int handle);

int rpncalc_push(int handle, double value);

int rpncalc_pop(int handle, double* topp);

int rpncalc_op(int handle, char op, double* topp);

int rpncalc_size(int handle, int* sizep);

int rpncalc_at(int handle, int index, double* valuep);

#endif // _RPNCALC_H_
