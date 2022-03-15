#include "gurobi_c++.h"
#include "parser.hpp"
using namespace std;

class Callback : public GRBCallback
{
public:
    GRBVar ***_x;
    int n;

    /**
       The constructor is used to get a pointer to the variables that are needed.
     */
    Callback(GRBVar ***x, int nb)
    {
        _x = x;
        n = nb;
    }

protected:
    void callback()
    {
        try
        {
            if (where == GRB_CB_MIPNODE)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    for (size_t j = 0; j < n; ++j)
                    {
                        for (size_t k = 1; k < n - 2; ++k)
                        {
                            double inVal = 0;
                            for (size_t l = 0; l < n; l++)
                            {
                                if (l != i)
                                    inVal += getNodeRel(_x[j][l][k + 1]);
                            }
                            double xVal = getNodeRel(_x[i][j][k]);
                            if (xVal > inVal)
                            {
                                cout << "Constraint not satisfied : xVal <= inVal. Adding this constraint." << endl;
                                GRBLinExpr _inVal = 0;
                                for (size_t l = 0; l < n; l++)
                                    if (l != i)
                                        _inVal += _x[j][l][k + 1];
                                addCut(_x[i][j][k] <= _inVal);
                            }
                        }
                    }
                }
            }
        }
        catch (GRBException e)
        {
            cout << "Error number: " << e.getErrorCode() << endl;
            cout << e.getMessage() << endl;
        }
        catch (...)
        {
            cout << "Error during callback" << endl;
        }
    }
};

int main(int argc,
         char *argv[])
{
    // parse and save the data
    vector<vector<int> > c = parse(argv[1]);
    int n = c.size();

    GRBVar ***x = nullptr;
    try
    {
        // --- Creation of the Gurobi environment ---
        cout << "--> Creating the Gurobi environment" << endl;
        GRBEnv env = GRBEnv(true);
        // env.set("LogFile", "mip1.log"); ///< prints the log in a file
        env.start();

        // --- Creation of the Gurobi model ---
        cout << "--> Creating the Gurobi model" << endl;
        GRBModel model = GRBModel(env);

        // --- Creation of the variables ---
        cout << "--> Creating the variables" << endl;

        x = new GRBVar **[n];

        for (size_t j = 0; j < n; ++j)
        {
            x[j] = new GRBVar *[n];
            for (size_t i = 0; i < n; ++i)
            {
                x[j][i] = new GRBVar[n];
                for (size_t k = 0; k < n; ++k)
                {
                    stringstream ss;
                    ss << "x(" << i << "," << j << "," << k << ")";
                    x[j][i][k] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, ss.str());
                }
            }
        }

        // --- Creation of the objective function ---
        cout << "--> Creating the objective function" << endl;
        GRBLinExpr obj = 0;
        for (size_t i = 0; i < n; ++i)
        {
            for (size_t j = 0; j < n; ++j)
            {
                for (size_t k = 0; k < n; ++k)
                {
                    if (i != j)
                        obj += c[j][i] * x[j][i][k];
                }
            }
        }
        model.setObjective(obj, GRB_MINIMIZE);

        // --- Creation of the constraints ---
        cout << "--> Creating the constraints" << endl;

        // Le sommet 0 est le seul pris en position 0
        GRBLinExpr arcDeb1 = 0;
        GRBLinExpr arcDeb2 = 0;
        for (size_t j = 1; j < n; ++j)
        {
            arcDeb1 += x[j][0][0];
            for (size_t i = 1; i < n; ++i)
            {
                arcDeb2 += x[j][i][0];
            }
        }
        model.addConstr(arcDeb1 == 1);
        model.addConstr(arcDeb2 == 0);

        // Respect 1 flot � tout niveau k
        for (size_t k = 0; k < n; ++k)
        {
            GRBLinExpr flot = 0;
            for (size_t j = 0; j < n; ++j)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != j)
                    {
                        flot += x[i][j][k];
                    }
                }
            }
            stringstream ss;
            ss << "Flot(" << k << ")";
            model.addConstr(flot == 1, ss.str());
        }

        // Respect flot � tout noeud (j,k)
        for (size_t k = 1; k < n; ++k)
        {
            for (size_t j = 1; j < n; ++j)
            {
                GRBLinExpr flot1 = 0;
                GRBLinExpr flot2 = 0;
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != j)
                    {
                        flot1 += x[j][i][k - 1];
                        flot2 += x[i][j][k];
                    }
                }
                stringstream ss;
                ss << "Flot(" << j << "," << k << ")";
                model.addConstr(flot1 == flot2, ss.str());
            }
        }

        // Respect flot pour chaque sommet j
        for (size_t j = 1; j < n; ++j)
        {
            GRBLinExpr flot1 = 0;
            GRBLinExpr flot2 = 0;
            for (size_t k = 0; k < n; ++k)
            {
                for (size_t i = 0; i < n; ++i)
                {
                    if (i != j)
                    {
                        flot1 += x[j][i][k];
                        flot2 += x[i][j][k];
                    }
                }
            }
            stringstream ss;
            ss << "Flot1(" << j << ")";
            model.addConstr(flot1 == 1, ss.str());
            ss << "Flot2(" << j << ")";
            model.addConstr(flot2 == 1, ss.str());
        }

        // On retourne sur le sommet 0 en derni�re position
        GRBLinExpr arcSor1 = 0;
        GRBLinExpr arcSor2 = 0;
        for (size_t i = 1; i < n; ++i)
        {
            arcSor1 += x[0][i][n - 1];
            for (size_t j = 1; j < n; ++j)
            {
                arcSor2 += x[j][i][n - 1];
            }
        }
        model.addConstr(arcSor1 == 1);
        model.addConstr(arcSor2 == 0);

        // Callback
        // model.set(GRB_IntParam_LazyConstraints, 1); // MANDATORYFOR LAZY CONSTRAINTS!
        Callback *cb = new Callback(x, n); // passing variable x to the solver callback
        model.setCallback(cb);             // adding the callback to the model

        /*
        //passer une seule fois sur chaque sommet

        for (size_t j = 0; j < n; ++j) {
            GRBLinExpr nbPass = 0;
                for (size_t i = 1; i < n; ++i) {
                    for (size_t k = 1; k < n; ++k)
                        if(i!=j) nbPass += x[j][i][k];
                }
            model.addConstr(nbPass == 1);
        }*/

        // Optimize model
        // --- Solver configuration ---
        cout << "--> Configuring the solver" << endl;
        model.set(GRB_DoubleParam_TimeLimit, 600.0); //< sets the time limit (in seconds)
        model.set(GRB_IntParam_Threads, 3);          //< limits the solver to single thread usage

        /*ATTENTION : J'ai mis 3 threads car le solveur disait "succ�s" avec des valeurs de X non binaires. A voir de pr�s*/

        // --- Solver launch ---
        cout << "--> Running the solver" << endl;
        model.optimize();
        model.write("model.lp"); //< Writes the model in a file

        // --- Solver results retrieval ---
        cout << "--> Retrieving solver results " << endl;

        int status = model.get(GRB_IntAttr_Status);
        if (status == GRB_OPTIMAL || (status == GRB_TIME_LIMIT && model.get(GRB_IntAttr_SolCount) > 0))
        {
            // the solver has computed the optimal solution or a feasible solution (when the time limit is reached before proving optimality)
            cout << "Succes! (Status: " << status << ")" << endl; //< prints the solver status (see the gurobi documentation)
            cout << "Runtime : " << model.get(GRB_DoubleAttr_Runtime) << " seconds" << endl;

            cout << "--> Printing results " << endl;
            // model.write("solution.sol"); //< Writes the solution in a file
            cout << "Objective value = " << model.get(GRB_DoubleAttr_ObjVal) << endl; //<gets the value of the objective function for the best computed solution (optimal if no time limit)

            int i = 0;
            int k = 0;
            for (size_t j = 0; j < n; ++j)
            {
                if (k == n)
                    break;
                // cout << x[j][i][k].get(GRB_DoubleAttr_X) << endl;
                if (x[j][i][k].get(GRB_DoubleAttr_X) == 1.0)
                {
                    cout << "ville " << i << " --> "
                         << "ville " << j << endl;
                    i = j;
                    if (i == 0) break;
                    j = -1;
                    k++;
                }
            }
        }
        else
        {
            // the model is infeasible (maybe wrong) or the solver has reached the time limit without finding a feasible solution
            cerr << "Fail! (Status: " << status << ")" << endl; //< see status page in the Gurobi documentation
        }
        delete cb;
    }
    catch (GRBException e)
    {
        cout << "Error code = " << e.getErrorCode() << endl;
        cout << e.getMessage() << endl;
    }
    catch (...)
    {
        cout << "Exception during optimization" << endl;
    }

    for (size_t j = 0; j < n; ++j)
    {
        for (size_t i = 0; i < n; ++i)
            delete[] x[j][i];
        delete[] x[j];
    }
    delete[] x;

    return 0;
}