//    LSD - Least Square Dating for etimating substitution rate and divergence dates
//    Copyright (C) <2015> <Thu-Hien To>

//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.

//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.

//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "confidence_interval.h"


void computeIC(double br,Pr* pr,Node** nodes,double* &T_left,double* &T_right,double* &H_left,double* &H_right,double* &HD_left,double* &HD_right,double &rho_left,double& rho_right,double* &other_rhos_left,double* &other_rhos_right){
    Node** nodes_new = new Node*[pr->nbBranches+1];
    int* tab = new int[pr->nbBranches+1];
    bool useSupport = false;
    int nbC = collapseTree(pr, nodes, nodes_new,tab,0,useSupport);//nbC is the number of internal nodes reduced
    Node** nodesReduced = new Node*[nbC+pr->nbBranches-pr->nbINodes+1];
    Pr* prReduced = new Pr(nbC,nbC+pr->nbBranches-pr->nbINodes);
    prReduced->copy(pr);
    collapseTreeReOrder( pr, nodes_new, prReduced, nodesReduced,tab);
    initConstraint(prReduced, nodesReduced);
    for (int i=0;i<pr->nbBranches+1;i++){
        delete nodes_new[i];
    }
    delete[] nodes_new;
    computeSuc_polytomy(prReduced, nodesReduced);
    double** B_simul = new double*[pr->nbSampling];
    for (int i=0;i<pr->nbSampling;i++){
        B_simul[i]=new double[prReduced->nbBranches+1];
    }
    std::default_random_engine generator;
    
    double minB=nodesReduced[1]->B;
    for (int i=2;i<=prReduced->nbBranches;i++){
        if (nodesReduced[i]->B<minB && nodesReduced[i]->B>0) {
            minB=nodesReduced[i]->B;
        }
    }
    int seqLength_forIC = pr->seqLength;//min(pr->seqLength,1000);
    double m = -log(prReduced->q*prReduced->q+1)/2;
    double sigma = sqrt(log(prReduced->q*prReduced->q+1));
    for (int i=1;i<=prReduced->nbBranches;i++){
        std::poisson_distribution<int> distribution(nodesReduced[i]->B*seqLength_forIC);
        std::lognormal_distribution<double> distributionlogNormal(m,sigma);
        for (int j=0;j<pr->nbSampling;j++){
            double coef = (double)distributionlogNormal(generator);
            B_simul[j][i]=(double)distribution(generator)*coef/(seqLength_forIC);
        }
    }
    double maxD = nodesReduced[prReduced->nbINodes]->D; // the most recent tip date
    for (int i = prReduced->nbINodes+1; i <= prReduced->nbBranches; i++){
        if (nodesReduced[i]->D > maxD){
            maxD = nodesReduced[i]->D;
        }
    }
    double** T_simul = new double*[pr->nbSampling];
    double** H_simul = new double*[pr->nbSampling];
    double** HD_simul = new double*[pr->nbSampling];
    double* rho_simul = new double[pr->nbSampling];
    vector<double>* other_rhos_simul = new vector<double>[pr->nbSampling];
    std::poisson_distribution<int> distribution(br*pr->seqLength);
    double* tipDates = new double[prReduced->nbBranches - prReduced->nbINodes + 1];
    for (int i=0;i<prReduced->nbBranches - prReduced->nbINodes + 1;i++) {
         tipDates[i] = nodesReduced[i + prReduced->nbINodes]->D;
    }
    computeNewVariance(prReduced,nodesReduced);
    for (int r=0;r<pr->nbSampling;r++){
        for (int j=0;j<=prReduced->nbBranches;j++){
            nodesReduced[j]->B=B_simul[r][j];
        }
        initialize_status(prReduced, nodesReduced);
        for (int g=1; g<=pr->ratePartition.size(); g++) {
            prReduced->multiplierRate[g] = pr->multiplierRate[g];
        }
        if (pr->ratePartition.size()>0){
            for (int i=0;i<=pr->ratePartition.size();i++) prReduced->multiplierRate[i] = 1;
        }
        if (pr->constraint) with_constraint_multirates(prReduced,nodesReduced,false);
        else without_constraint_multirates(prReduced,nodesReduced,false);
        T_simul[r]=new double[prReduced->nbBranches+1];
        for (int j=0;j<=prReduced->nbBranches;j++) T_simul[r][j]=nodesReduced[j]->D;
        rho_simul[r]=prReduced->rho;
        for (int g=1; g<=pr->ratePartition.size(); g++) {
            other_rhos_simul[r].push_back(prReduced->rho*prReduced->multiplierRate[g]);
        }
        calculate_tree_height(prReduced,nodesReduced);
        H_simul[r]=new double[prReduced->nbBranches+1];
        HD_simul[r]=new double[prReduced->nbBranches+1];
        for (int j=0;j<=prReduced->nbBranches;j++){
            H_simul[r][j]=nodesReduced[j]->H;
            HD_simul[r][j]=nodesReduced[j]->HD;
        }
        delete[] B_simul[r];
    }
    delete[] B_simul;
    delete prReduced;
    for (int i=0;i<nbC+pr->nbBranches-pr->nbINodes+1;i++){
        delete nodesReduced[i];
    }
    delete[] nodesReduced;
    sort(rho_simul,pr->nbSampling);
    rho_left=rho_simul[int(0.025*pr->nbSampling)];
    rho_right=rho_simul[pr->nbSampling-int(0.025*pr->nbSampling)-1];
    if (pr->rho<rho_left) rho_left=pr->rho;
    if (pr->rho>rho_right) rho_right=pr->rho;
    double* T_sort = new double[pr->nbSampling];
    double* H_sort = new double[pr->nbSampling];
    double* HD_sort = new double[pr->nbSampling];
    for (int i=0;i<=pr->nbBranches;i++){
        if (tab[i]!=-1) {
            for (int j=0;j<pr->nbSampling;j++) {
                T_sort[j]=T_simul[j][tab[i]];
                H_sort[j]=H_simul[j][tab[i]];
                HD_sort[j]=HD_simul[j][tab[i]];
            }
            sort(T_sort,pr->nbSampling);
            sort(H_sort,pr->nbSampling);
            sort(HD_sort,pr->nbSampling);
            
            T_left[i]=T_sort[int(0.025*pr->nbSampling)];
            if (T_left[i]>nodes[i]->D) T_left[i]=nodes[i]->D;
            T_right[i]=T_sort[pr->nbSampling-int(0.025*pr->nbSampling)-1];
            if (T_right[i]<nodes[i]->D) T_right[i]=nodes[i]->D;
            
            H_left[i]=H_sort[int(0.025*pr->nbSampling)];
            if (H_left[i]>nodes[i]->H) H_left[i]=nodes[i]->H;
            H_right[i]=H_sort[pr->nbSampling-int(0.025*pr->nbSampling)-1];
            if (H_right[i]<nodes[i]->H) H_right[i]=nodes[i]->H;
            
            HD_left[i]=HD_sort[int(0.025*pr->nbSampling)];
            if (HD_left[i]>nodes[i]->HD) HD_left[i]=nodes[i]->HD;
            HD_right[i]=HD_sort[pr->nbSampling-int(0.025*pr->nbSampling)-1];
            if (HD_right[i]<nodes[i]->HD) HD_right[i]=nodes[i]->HD;
        }
    }
    double* other_rhos_simul_sort = new double[pr->nbSampling];
    for (int g=1;g<=pr->ratePartition.size();g++){
        for (int r=0;r<pr->nbSampling;r++) {
            other_rhos_simul_sort[r]=other_rhos_simul[r][g-1];
        }
        sort(other_rhos_simul_sort,pr->nbSampling);
        other_rhos_left[g]=other_rhos_simul_sort[int(0.025*pr->nbSampling)];
        if (other_rhos_left[g]>pr->rho*pr->multiplierRate[g]) other_rhos_left[g]=pr->rho*pr->multiplierRate[g];
        other_rhos_right[g]=other_rhos_simul_sort[pr->nbSampling-int(0.025*pr->nbSampling)-1];
        if (other_rhos_right[g]<pr->rho*pr->multiplierRate[g]) other_rhos_right[g]=pr->rho*pr->multiplierRate[g];
    }
    delete[] tab;
    delete[] rho_simul;
    delete[] T_sort;
    delete[] H_sort;
    delete[] HD_sort;
    delete[] other_rhos_simul_sort;
    delete[] other_rhos_simul;
    for (int i=0;i<pr->nbSampling;i++){
        delete[] T_simul[i];
        delete[] H_simul[i];
        delete[] HD_simul[i];
    }
    delete[] T_simul;
    delete[] H_simul;
    delete[] HD_simul;
}

void output(double br,int y, Pr* pr,Node** nodes,ostream& f,ostream& tree1,ostream& tree2,ostream& tree3){
    if (pr->outlier.size()>0){
        std::ostringstream oss;
        oss<<"- There are "<<pr->outlier.size()<<" nodes that are considered as outliers and were excluded from the analysis:\n";
        for (int i=0;i<pr->outlier.size();i++){
            oss<<"    "<<nodes[pr->outlier[i]]->L<<", suggesting date "<<nodes[pr->outlier[i]]->D<<"\n";
        }
        pr->resultMessage.push_back(oss.str());
    }
    if (!pr->constraint && pr->ci) {
        std::ostringstream oss;
        oss<<"- Confidence intervals are not warranted under non-constraint mode.\n";
        pr->warningMessage.push_back(oss.str());
    }
    if (pr->relative) {
        std::ostringstream oss;
        ostringstream tMRCA,tLeaves;
        if (pr->outDateFormat==2){
            tMRCA<<realToYearMonthDay(pr->mrca);
            tLeaves<<realToYearMonthDay(pr->leaves);
        } else {
            tMRCA<<pr->mrca;
            tLeaves<<pr->leaves;
        }
        oss<<"- The results correspond to the estimation of relative dates when T[mrca]="<<tMRCA.str()<<" and T[tips]="<<tLeaves.str()<<"\n";
        pr->warningMessage.push_back(oss.str());
    }
    ostringstream tMRCA;
    if (pr->outDateFormat==2){
        tMRCA<<realToYearMonthDay(nodes[0]->D);
    } else {
        tMRCA<<nodes[0]->D;
    }
    if (pr->ratePartition.size()==0) {
        std::ostringstream oss;
        oss<<"- Dating results:\n";
        oss<<" rate "<<pr->rho<<", tMRCA "<<tMRCA.str()<<", objective function "<<pr->objective<<"\n";
        pr->resultMessage.push_back(oss.str());
    }
    else{
        std::ostringstream oss;
        oss<<"- Dating results:\n";
        if (pr->multiplierRate[0]!=-1){
            oss<<" rate "<<pr->rho<<", ";
        }
        for (int i=1; i<=pr->ratePartition.size(); i++) {
            if (pr->multiplierRate[i]>0)
                oss<<"rate "<<pr->ratePartition[i-1]->name.c_str()<<" "<<pr->rho*pr->multiplierRate[i]<<", ";
        }
        oss<<"tMRCA "<<tMRCA.str()<<", objective function "<<pr->objective<<"\n";
        pr->resultMessage.push_back(oss.str());
    }
    
    //variance = 2
    if (pr->variance==2){
        cout<<"Re-estimating using variances based on the branch lengths of the first run ...\n";
        if (pr->estimate_root=="") {
            computeNewVariance(pr,nodes);
            if (pr->constraint) {
                with_constraint_multirates(pr,nodes,false);
            }
            else{
                without_constraint_multirates(pr,nodes,false);
            }
        }
        else{
            computeNewVarianceEstimateRoot(pr,nodes);
            if (pr->constraint){
               with_constraint_active_set_lambda_multirates(br,pr, nodes,true);
            }
            else{
                without_constraint_active_set_lambda_multirates(br,pr, nodes,true);
            }
        }
        std::ostringstream oss;
        oss<<"- Results of the second run using variances based on the branch lengths of the first run:\n";
        pr->resultMessage.push_back(oss.str());
        ostringstream tMRCA;
        if (pr->outDateFormat==2){
            tMRCA<<realToYearMonthDay(nodes[0]->D);
        } else {
            tMRCA<<nodes[0]->D;
        }
        if (pr->ratePartition.size()==0) {
            std::ostringstream oss;
            oss<<" rate "<<pr->rho<<", tMRCA "<<tMRCA.str()<<", objective function "<<pr->objective<<"\n";
            pr->resultMessage.push_back(oss.str());
        }
        else{
            std::ostringstream oss;
            if (pr->multiplierRate[0]!=-1){
                oss<<" rate "<<pr->rho<<", ";
            }
            for (int i=1; i<=pr->ratePartition.size(); i++) {
                if (pr->multiplierRate[i]>0)
                    oss<<"rate "<<pr->ratePartition[i-1]->name.c_str()<<" "<<pr->rho*pr->multiplierRate[i]<<", ";
            }
            oss<<"tMRCA "<<tMRCA.str()<<", objective function "<<pr->objective<<"\n";
            pr->resultMessage.push_back(oss.str());
        }
    }
    
    //Replace branche length by time-scaled lengths
    if (pr->ratePartition.size()==0){
        for (int i=1;i<=pr->nbBranches;i++){
            nodes[i]->B=pr->rho*(nodes[i]->D-nodes[nodes[i]->P]->D);
        }
    }
    else {
        for (int i=1;i<=pr->nbBranches;i++){
                int g = nodes[i]->rateGroup;
                if (g==0) {
                    nodes[i]->B=pr->rho*(nodes[i]->D-nodes[nodes[i]->P]->D);
                }
                else{
                    nodes[i]->B=pr->rho*pr->multiplierRate[g]*(nodes[i]->D-nodes[nodes[i]->P]->D);
                }
        }
    }
    //Warning message without option -c
    
    if (!pr->constraint){
        int count=0;
        for (int i=1;i<=pr->nbBranches;i++){
            if (nodes[i]->D<nodes[nodes[i]->P]->D) count++;
        }
        
        if (count>0) {
            std::ostringstream oss;
            oss<<"- Number of violated temporal constraints (nodes having date smaller than the one of its parent):  "<<count<<" ("<<(count*100)/(double)pr->nbBranches<<"%%). Try option -c to impose temporal constraints on the estimated trees.\n";
            pr->warningMessage.push_back(oss.str());
        }
        
    }
    calculate_tree_height(pr,nodes);
    //Calculate confidence intervals
    if (!pr->ci){
        tree1<<"tree "<<y<<" = ";
        tree1<<nexus(0,pr,nodes).c_str();
        tree2<<"tree "<<y<<" = ";
        tree2<<nexusDate(0,pr,nodes).c_str();int n=0;
        tree3<<newick(0,0,pr,nodes,n).c_str();
    }
    else{
        double* T_min = new double[pr->nbBranches+1];
        double* T_max = new double[pr->nbBranches+1];
        double* H_min = new double[pr->nbBranches+1];
        double* H_max = new double[pr->nbBranches+1];
        double* HD_min = new double[pr->nbBranches+1];
        double* HD_max = new double[pr->nbBranches+1];
        double rho_left,rho_right;
        double* other_rhos_left = new double[pr->ratePartition.size()+1];
        double* other_rhos_right = new double[pr->ratePartition.size()+1];
        cout<<"Computing confidence intervals using sequence length "<<pr->seqLength<<" and a lognormal relaxed clock with mean 1, standard deviation "<<pr->q<<" (settable via option q)"<<endl;
        computeIC(br,pr,nodes,T_min,T_max,H_min,H_max,HD_min,HD_max,rho_left,rho_right,other_rhos_left,other_rhos_right);
        
        std::ostringstream oss;
        oss<<"- Results with confidence intervals:\n";
        pr->resultMessage.push_back(oss.str());
        ostringstream tMRCA,tmin,tmax;
        if (pr->outDateFormat==2){
            tMRCA<<realToYearMonthDay(nodes[0]->D);
            tmin<<realToYearMonthDay(T_min[0]);
            tmax<<realToYearMonthDay(T_max[0]);
        } else {
            tMRCA<<nodes[0]->D;
            tmin<<T_min[0];
            tmax<<T_max[0];
        }
        if (pr->ratePartition.size()==0) {
            std::ostringstream oss;
            oss<<" rate "<<pr->rho<<" ["<<rho_left<<"; "<<rho_right<<"], tMRCA "<<tMRCA.str()<<" ["<<tmin.str()<<"; "<<tmax.str()<<"], objective function "<<pr->objective<<"\n";
            pr->resultMessage.push_back(oss.str());
        }
        else{
            std::ostringstream oss;
            if (pr->multiplierRate[0]!=-1){
                oss<<" rate "<<pr->rho<<" ["<<rho_left<<"; "<<rho_right<<"], ";
            }
            for (int i=1; i<=pr->ratePartition.size(); i++) {
                if (pr->multiplierRate[i]>0) oss<<"rate "<<pr->ratePartition[i-1]->name.c_str()<<" "<<pr->rho*pr->multiplierRate[i]<<" ["<<other_rhos_left[i]<<"; "<<other_rhos_right[i]<<"], ";
            }
            oss<<"tMRCA "<<tMRCA.str()<<" ["<<tmin.str()<<"; "<<tmax.str()<<"], objective function "<<pr->objective<<"\n";
            pr->resultMessage.push_back(oss.str());
        }
        
        delete[] other_rhos_left;
        delete[] other_rhos_right;
        
        tree1<<"tree "<<y<<" = ";
        tree2<<"tree "<<y<<" = ";
        tree1<<nexusIC(0,pr,nodes,T_min,T_max,H_min,H_max).c_str();
        tree2<<nexusICDate(0,pr,nodes,T_min,T_max,HD_min,HD_max).c_str();int n=0;
        tree3<<newick(0,0,pr,nodes,n).c_str();
        
        delete[] T_min;
        delete[] T_max;
        delete[] H_min;
        delete[] H_max;
    }
    
    if (pr->rho==pr->rho_min) {
        std::ostringstream oss;
        oss<<"- The estimated rate reaches the given lower bound. To change the lower bound, use option -t.\n";
        pr->warningMessage.push_back(oss.str());
    }
    if ((pr->warningMessage).size()>0){
        f<<"*WARNINGS:\n";
        cout<<"*WARNINGS:\n";
        for (int i=0;i<pr->warningMessage.size();i++){
            f<<string(pr->warningMessage[i]).c_str();
           cout<<string(pr->warningMessage[i]).c_str();
        }
        
    }
    
    f<<"*RESULTS:\n";
    cout<<"*RESULTS:\n";
    for (int i=0;i<pr->resultMessage.size();i++){
        f<<string(pr->resultMessage[i]).c_str();
        cout<<string(pr->resultMessage[i]).c_str();
    }

}
