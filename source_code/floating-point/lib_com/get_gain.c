/*====================================================================================
    EVS Codec 3GPP TS26.443 Nov 13, 2018. Version 12.11.0 / 13.7.0 / 14.3.0 / 15.1.0
  ====================================================================================*/

#include "options.h"
#include "prot.h"


/*----------------------------------------------------------------------------------*
*  get_gain()
*
*
*----------------------------------------------------------------------------------*/

float get_gain(     /* output: codebook gain (adaptive or fixed)    */
    float x[],        /* input : target signal                        */
    float y[],        /* input : filtered codebook excitation         */
    int n,            /* input : segment length                       */
    float *en_y       /* output: energy of y (sum of y[]^2, optional) */
)
{
    float corr = 0.0f, ener = 1e-6f;
    short i;

    for (i = 0; i < n; i++)
    {
        corr += x[i]*y[i];
        ener += y[i]*y[i];
    }

    if (en_y)
    {
        *en_y = ener;
    }

    return(corr/ener);
}
