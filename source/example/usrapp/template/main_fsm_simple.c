/*****************************************************************************
 *   Copyright(C)2009-2019 by VSF Team                                       *
 *                                                                           *
 *  Licensed under the Apache License, Version 2.0 (the "License");          *
 *  you may not use this file except in compliance with the License.         *
 *  You may obtain a copy of the License at                                  *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 *  Unless required by applicable law or agreed to in writing, software      *
 *  distributed under the License is distributed on an "AS IS" BASIS,        *
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
 *  See the License for the specific language governing permissions and      *
 *  limitations under the License.                                           *
 *                                                                           *
 ****************************************************************************/
/*============================ INCLUDES ======================================*/
#include "vsf.h"
#include <stdio.h>

/*============================ MACROS ========================================*/



/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
declare_fsm(user_task_t)
declare_fsm(user_sub_task_t)

def_fsm(user_sub_task_t,
    def_params(
        uint32_t cnt;
    ));


def_fsm(user_task_t,
    def_params(
        vsf_sem_t *psem;
        uint32_t cnt;
        
        vsf_task(user_sub_task_t) print_task;
    ));
    
#if VSF_OS_RUN_MAIN_AS_THREAD != ENABLED
declare_fsm(user_task_b_t)
def_fsm(user_task_b_t,
    def_params(
        vsf_sem_t *psem;
        uint8_t state;
    ));
#endif

/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/
static NO_INIT vsf_sem_t user_sem;
/*============================ PROTOTYPES ====================================*/
/*============================ IMPLEMENTATION ================================*/


fsm_initialiser(user_sub_task_t,
    args(
        uint8_t chCount
    ))
    init_body(
        this.cnt = chCount;
    )


implement_fsm(user_sub_task_t) 
    def_states(PRINT_INFO);
    
    body(                                                                       
        //! this can be ignored
        on_start(
            update_state_to(PRINT_INFO);
        )
        
        state(PRINT_INFO) {
            printf("receive semaphore from main...[%08x]\r\n", this.cnt++);
            fsm_cpl();
        }
    )



fsm_initialiser(user_task_t,
    args(
        vsf_sem_t *ptSEM
    ))
    init_body(
        this.cnt = 0;
        this.psem = ptSEM;
    )


/*----------------------------------------------------------------------------*
 * Example of unprotected fsm                                                 *
 * NOTE: you can debug content between body_begin() and body_end()            *
 *----------------------------------------------------------------------------*/
implement_fsm(user_task_t) 
{
    def_states(WAIT_FOR_SEM, CALL_SUB_TO_PRINT);
    
    body_begin();                                                               

    /*! this can be ignored
    on_start(
        update_state_to(WAIT_FOR_SEM);
    )
    */

    state(WAIT_FOR_SEM) {
        vsf_task_wait_until(
            vsf_sem_pend(this.psem, -1){                                        //!< wait for semaphore forever
                
                init_fsm(user_sub_task_t, &this.print_task, args(this.cnt));    //!< init sub fsm
                transfer_to(CALL_SUB_TO_PRINT);                                 //!< tranfer to next state
            }
        );
    }
    
    state(CALL_SUB_TO_PRINT) {
        if (fsm_rt_cpl == call_fsm(user_sub_task_t, &this.print_task)) {
            //! fsm complete
            this.cnt = this.print_task.cnt;                                     //!< read param value
            reset_fsm();
        }
    }
    
    body_end();
}

#if VSF_OS_RUN_MAIN_AS_THREAD != ENABLED

/*! \IMPORTANT You cannot ignore fsm_initialiser at any time
 */
fsm_initialiser(user_task_b_t,
    args(
        vsf_sem_t *ptSEM
    ))
    init_body(
        this.psem = ptSEM;
    )

/*----------------------------------------------------------------------------*
 * Example of protected fsm                                                   *
 * NOTE: content in body() is protected from debug                            *
 *----------------------------------------------------------------------------*/
implement_fsm(user_task_b_t) 
    def_states(PRINT, DELAY);
    
    body(
        on_start(
            update_state_to(PRINT);
        )
        
        state(PRINT){
            printf("hello world! \r\n");
            update_state_to(DELAY);                                             //!< transfer to DELAY without yielding...
        }
        
        state(DELAY){
            vsf_task_wait_until(
                vsf_delay(10000){                                               //!< wait 10s
                    vsf_sem_post(this.psem);                                    //!< post a semaphore
                    reset_fsm();                                                //!< reset fsm
                }
            );
        }
    )
#endif

static void system_init(void)
{
    extern void stdout_init(void);
    stdout_init();
}

int main(void)
{
    static_task_instance(
        features_used(
            mem_sharable( )
            mem_nonsharable( )
        )
    )
    
    system_init();
    
    //! initialise semaphore
    vsf_sem_init(&user_sem, 0); 
    
    //! start a user task
    do {
        static NO_INIT user_task_t __user_task;
        init_fsm(user_task_t, 
                ref_obj_as(__user_task, fsm(user_task_t)), 
                args(&user_sem));
        start_fsm(user_task_t, &__user_task, vsf_priority_0);
    } while(0);

#if VSF_OS_RUN_MAIN_AS_THREAD == ENABLED
    while(1) {
        printf("hello world! \r\n");
        vsf_delay(10000);
        vsf_sem_post(&user_sem);            //!< post a semaphore
    }
#else
    //! in this case, we only use main to initialise vsf_tasks



    //! start a user task b
    do {
        static NO_INIT user_task_b_t __user_task_b;
        init_fsm(user_task_b_t, 
                ref_obj_as(__user_task_b, fsm(user_task_b_t)), 
                args(&user_sem));
        start_fsm(user_task_b_t, &__user_task_b, vsf_priority_0);
    } while(0);
    
    return 0;
#endif
}