/*==============================================================================
| sighandle.h - catch some crash crash conditions so that we can log and exit
| 
| coded and tested under linux 2.0.23, 2.0.26, stealth kernel 2.0.29
|  by graham the ollis <ollisg@ns.arizona.edu>
|
| your free to modify and distribute this program as long as this header is
| retained, source code is made *freely* available and you document your 
| changes in some readable manner.
==============================================================================*/

#ifndef SIGHANDLE_H
#define SIGHANDLE_H

/*==============================================================================
| prototypes
==============================================================================*/

void	handle();
void	sig_handler(int sig);

#endif /* NETL_H */