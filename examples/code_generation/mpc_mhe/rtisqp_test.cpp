#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <cmath>

using namespace std;

// acado codegen
#include "acado_common.h"
#include "acado_auxiliary_functions.h"

#define NX          ACADO_NX	/* number of differential states */
#define NXA         ACADO_NXA	/* number of alg. states */
#define NU          ACADO_NU	/* number of control inputs */
#define N          	ACADO_N		/* number of control intervals */
#define NY			ACADO_NY	/* number of references, nodes 0..N - 1 */
#define NYN			ACADO_NYN
#define NUM_STEPS   100			/* number of simulation steps */
#define VERBOSE     1			/* show iterations: 1, silent: 0 */

/* Differential states:
 * s, d, deltapsi, psidot, vx, vy, kappac
 *
 * Controls:
 * Fyf, Fx
 */

ACADOvariables acadoVariables;
ACADOworkspace acadoWorkspace;

struct trajectory{
  std::vector<float> t;
  std::vector<float> kappac;
  std::vector<float> s;
  std::vector<float> d;
  std::vector<float> X;
  std::vector<float> Y;
  std::vector<float> deltapsi;
  std::vector<float> psi;
  std::vector<float> psidot;
  std::vector<float> vx;
  std::vector<float> vy;
  std::vector<float> ax;
  std::vector<float> ay;
  std::vector<float> Fyf;
  std::vector<float> Fxf;
  std::vector<float> Fyr;
  std::vector<float> Fxr;
  std::vector<float> Fx;
  std::vector<float> alphaf;
  std::vector<float> alphar;
  std::vector<float> Crtilde;
  std::vector<float> delta;
  float valid;
  float coll_cost;
  float cost;
};


int main()
{
    unsigned  i, j, iter;
    acado_timer t;
    real_t t1, t2;
    real_t fdbSum = 0.0;
    real_t prepSum = 0.0;
    int status;

    t1 = t2 = 0;

    // Reset all solver memory
    memset(&acadoWorkspace, 0, sizeof( acadoWorkspace ));
    memset(&acadoVariables, 0, sizeof( acadoVariables ));

    // Initialize the solver
    acado_initializeSolver();

    // Prepare a consistent initial guess
    for (i = 0; i < N + 1; ++i)
    {
        acadoVariables.x[i * NX + 0] = 1;
        acadoVariables.x[i * NX + 1] = sqrt(1.0 - 0.1 * 0.1);
        acadoVariables.x[i * NX + 2] = 0.9;
        acadoVariables.x[i * NX + 3] = 0;
        acadoVariables.x[i * NX + 4] = 0;
        acadoVariables.x[i * NX + 5] = 0;
    }

    //
    // Prepare references
    //

    for (i = 0; i < N; ++i)
    {
        acadoVariables.y[i * NY + 0] = 0; // x
        acadoVariables.y[i * NY + 1] = 1.0; // y
        acadoVariables.y[i * NY + 2] = 0; // w
        acadoVariables.y[i * NY + 3] = 0;
        acadoVariables.y[i * NY + 4] = 0;
        acadoVariables.y[i * NY + 5] = 0;
        acadoVariables.y[i * NY + 6] = 0; // u
    }

    acadoVariables.yN[ 0 ] = 0; // x
    acadoVariables.yN[ 1 ] = 1.0; // y
    acadoVariables.yN[ 2 ] = 0; // w
    acadoVariables.yN[ 3 ] = 0;
    acadoVariables.yN[ 4 ] = 0;
    acadoVariables.yN[ 5 ] = 0;

    //
    // Current state feedback
    //
    for (i = 0; i < NX; ++i)
        acadoVariables.x0[ i ] = acadoVariables.x[ i ];

    //
    // Logger initialization
    //
    std::vector< std::vector< double > > log;

    log.resize(NUM_STEPS);
    for (i = 0; i < log.size(); ++i)
        log[ i ].resize(NX + NXA + NU + 5, 0.0);


    //
    // Warm-up the solver
    //
    acado_preparationStep();

    //
    // Real-time iterations loop
    //
    for( iter = 0; iter < NUM_STEPS; iter++ )
    {
        acado_tic( &t );
        status = acado_feedbackStep( );
        t2 = acado_toc( &t );

#if VERBOSE
        acado_printDifferentialVariables();
        acado_printControlVariables();
#endif // VERBOSE

        if ( status )
        {
            cout << "Iteration:" << iter << ", QP problem! QP status: " << status << endl;

            break;
        }

        //
        // Logging
        //

        for(i = 0; i < NX; i++)
            log[ iter ][ i ] = acadoVariables.x[ i ];
        for(j = 0;  i < NX + NXA + NU; i++, j++)
            log[ iter ][ i ] = acadoVariables.u[ j ];

        log[ iter ][ i++ ] = t1;
        log[ iter ][ i++ ] = t2;
        log[ iter ][ i++ ] = acado_getObjective();
        log[ iter ][ i++ ] = acado_getKKT();
        log[ iter ][ i++ ] = acado_getNWSR();

#if VERBOSE
        cout	<< "Iteration #" << setw( 4 ) << iter
                << ", KKT value: " << scientific << acado_getKKT()
                << ", objective value: " << scientific << acado_getObjective()
                << endl;
#endif // VERBOSE

        //
        // Prepare for the next iteration
        //

        // In this simple example, we feed the NMPC with an ideal feedback signal
        // i.e. what NMPC really expects in the next sampling interval
        for (i = 0; i < NX; ++i)
            acadoVariables.x0[ i ] = acadoVariables.x[NX + i];

        // Shift states and control and prepare for the next iteration
        acado_shiftStates(2, 0, 0);
        acado_shiftControls( 0 );

        // tmp test reset initial guess
        for (i = 0; i < N + 1; ++i)
        {
            acadoVariables.x[i * NX + 0] = 1;
            acadoVariables.x[i * NX + 1] = sqrt(1.0 - 0.1 * 0.1);
            acadoVariables.x[i * NX + 2] = 0.9;
            acadoVariables.x[i * NX + 3] = 0;
            acadoVariables.x[i * NX + 4] = 0;
            acadoVariables.x[i * NX + 5] = 0;
        }
        // end tmp

        acado_tic( &t );
        acado_preparationStep();
        t1 = acado_toc( &t );

        //
        // More logging...
        //
        prepSum += t1;
        fdbSum += t2;
    }

#if VERBOSE
    cout << "Average feedback time:    " << scientific << fdbSum / NUM_STEPS * 1e6 << " microseconds" << endl;
    cout << "Average preparation time: " << scientific << prepSum / NUM_STEPS * 1e6 << " microseconds" << endl;
#endif // VERBOSE

    //
    // Save log to a file
    //
    ofstream dataLog( "./rtisqp_test_sim_log.txt" );
    if ( dataLog.is_open() )
    {
        for (i = 0; i < log.size(); i++)
        {
            for (j = 0; j < log[ i ].size(); j++)
                dataLog << log[ i ][ j ] << " ";
            dataLog << endl;
        }

        dataLog.close();
    }
    else
    {
        cout << "Log file could not be opened" << endl;

        return 1;
    }

    // For debugging
    if ( status )
    {
        cout << "Solver failed!" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
