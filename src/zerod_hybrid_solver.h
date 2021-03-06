/****************************************************************************
 * Solves the 0d numerical equations using a hybrid approach of steady state
 * nonlinear solver and ODE solver 
***************************************************************************
*/

#ifndef OMKM_HYBRID_SOLVER_H
#define OMKM_HYBRID_SOLVER_H

#include <vector>

#include "NonLinearSolver.h"
#include "KIN_Solver.h"
#include "cantera/numerics/IDA_Solver.h"

namespace HeteroCt {

class HybridSolver : FuncEval
{
public:
    HybridSolver();

    HybridSolver(PFR1d* pfr);

    ~HybridSolver();

    //! @name Methods to set up a simulation.
    //@{

    //! Set initial time. Default = 0.0 s. Restarts integration from this time
    //! using the current mixture state as the initial condition.
    void setInitialTime(double time);

    //! Set the maximum time step.
    void setMaxTimeStep(double maxstep);

    //! Set the maximum number of error test failures permitted by the CVODES
    //! integrator in a single time step.
    void setMaxErrTestFails(int nmax);

    //! Set the relative and absolute tolerances for the integrator.
    void setTolerances(double rtol, double atol);

    //! Set the relative and absolute tolerances for integrating the
    //! sensitivity equations.
    void setSensitivityTolerances(double rtol, double atol);

    /! Relative tolerance.
     doublereal rtol() {
         return m_rtol;
     }

     //! Absolute integration tolerance
     doublereal atol() {
         return m_atols;
     }

     //! Relative sensitivity tolerance
     doublereal rtolSensitivity() const {
         return m_rtolsens;
     }

     //! Absolute sensitivity tolerance
     doublereal atolSensitivity() const {
         return m_atolsens;
     }

     /**
      * Advance the state of all reactors in time. Take as many internal
      * timesteps as necessary to reach *time*.
      * @param time Time to advance to (s).
      */
     void advance(doublereal time);

     //! Advance the state of all reactors in time.
     double step();

     //@}
     //
     //! Set the solver to steady state
     void setSteadyState(bool ss_flag);

     //! Solve
     void solve();

     //! Add the reactor *r* to this reactor network.
     void addReactor(Reactor& r);

     //! Return a reference to the *n*-th reactor in this network. The reactor
     //! indices are determined by the order in which the reactors were added
     //! to the reactor network.
     Reactor& reactor(int n) {
         return *m_reactors[n];
     }

     //! Returns `true` if verbose logging output is enabled.
     bool verbose() const {
         return m_verbose;
     }

     //! Enable or disable verbose logging while setting up and integrating the
     //! reactor network.
     void setVerbose(bool v = true) {
         m_verbose = v;
         suppressErrors(!m_verbose);
     }

     //! Return a reference to the integrator.
     Integrator& integrator() {
         return *m_integ;
     }

     //! Update the state of all the reactors in the network to correspond to
     //! the values in the solution vector *y*.
     void updateState(doublereal* y);

     //! Return the sensitivity of the *k*-th solution component with respect to
     //! the *p*-th sensitivity parameter.
     /*!
      *  The sensitivity coefficient \f$ S_{ki} \f$ of solution variable \f$ y_k
      *  \f$ with respect to sensitivity parameter \f$ p_i \f$ is defined as:
      *
      *  \f[ S_{ki} = \frac{1}{y_k} \frac{\partial y_k}{\partial p_i} \f]
      *
      *  For reaction sensitivities, the parameter is a multiplier on the forward
      *  rate constant (and implicitly on the reverse rate constant for
      *  reversible reactions) which has a nominal value of 1.0, and the
      *  sensitivity is nondimensional.
      *
      *  For species enthalpy sensitivities, the parameter is a perturbation to
      *  the molar enthalpy of formation, such that the dimensions of the
      *  sensitivity are kmol/J.
      */
     double sensitivity(size_t k, size_t p);

     //! Return the sensitivity of the component named *component* with respect to
     //! the *p*-th sensitivity parameter.
     //! @copydetails ReactorNet::sensitivity(size_t, size_t)
     double sensitivity(const std::string& component, size_t p, int reactor=0) {
         size_t k = globalComponentIndex(component, reactor);
         return sensitivity(k, p);
     }

     //! Evaluate the Jacobian matrix for the reactor network.
     /*!
      *  @param[in] t Time at which to evaluate the Jacobian
      *  @param[in] y Global state vector at time *t*
      *  @param[out] ydot Time derivative of the state vector evaluated at *t*.
      *  @param[in] p sensitivity parameter vector (unused?)
      *  @param[out] j Jacobian matrix, size neq() by neq().
      */
     void evalJacobian(doublereal t, doublereal* y,
                       doublereal* ydot, doublereal* p, Array2D* j);
 

     // overloaded methods of class FuncEval
     virtual size_t neq() {
         return m_nv;
     }

     
     virtual void eval(doublereal t, doublereal* y,
                       doublereal* ydot, doublereal* p);

     virtual void getState(doublereal* y);

     virtual size_t nparams() {
         return m_sens_params.size();
     }

     //! Return the index corresponding to the component named *component* in the
     //! reactor with index *reactor* in the global state vector for the
     //! reactor network.
     size_t globalComponentIndex(const std::string& component, size_t reactor=0);

     //! Return the name of the i-th component of the global state vector. The
     //! name returned includes both the name of the reactor and the specific
     //! component, e.g. `'reactor1: CH4'`.
     std::string componentName(size_t i) const;

     //! Used by Reactor and Wall objects to register the addition of
     //! sensitivity parameters so that the ReactorNet can keep track of the
     //! order in which sensitivity parameters are added.
     //! @param name A name describing the parameter, e.g. the reaction string
     //! @param value The nominal value of the parameter
     //! @param scale A scaling factor to be applied to the sensitivity
     //!     coefficient
     //! @returns the index of this parameter in the vector of sensitivity
     //!     parameters (global across all reactors)
     size_t registerSensitivityParameter(const std::string& name, double value,
                                         double scale);

     //! The name of the p-th sensitivity parameter added to this ReactorNet.
     const std::string& sensitivityParameterName(size_t p) {
         return m_paramNames.at(p);
     }

     //! Reinitialize the integrator. Used to solve a new problem (different
     //! initial conditions) but with the same configuration of the reactor
     //! network. Can be called manually, or automatically after calling
     //! setInitialTime or modifying a reactor's contents.
     void reinitialize();

     //! Called to trigger integrator reinitialization before further
     //! integration.
     void setNeedsReinit() {
         m_integrator_init = false;
     }

     //! Set the maximum number of internal integration time-steps the
     //! integrator will take before reaching the next output time
     //! @param nmax The maximum number of steps, setting this value
     //!             to zero disables this option.
     virtual void setMaxSteps(int nmax) {
         m_integ->setMaxSteps(nmax);
     }

     //! Returns the maximum number of internal integration time-steps the
     //!  integrator will take before reaching the next output time
     //!
     virtual int maxSteps() {
         return m_integ->maxSteps();
     }









protected:
    //! Initialize the reactor network. Called automatically the first time
    //! advance or step is called.
    void initialize();

    std::vector<Reactor*> m_reactors;
    std::unique_ptr<Integrator> m_integ;
    std::unique_ptr<NonLinearSolver> m_nonlinsol;
    doublereal m_time;
    bool m_init;
    bool m_integrator_init; //!< True if integrator initialization is current
    size_t m_nv;
    bool m_state;

    //! m_start[n] is the starting point in the state vector for reactor n
    std::vector<size_t> m_start;

    vector_fp m_atol;
    doublereal m_rtol, m_rtolsens;
    doublereal m_atols, m_atolsens;

    //! Maximum integrator internal timestep. Default of 0.0 means infinity.
    doublereal m_maxstep;

    int m_maxErrTestFails;
    bool m_verbose;

    //! Names corresponding to each sensitivity parameter
    std::vector<std::string> m_paramNames;

    vector_fp m_ydot;

    
}; 

} 

#endif 
