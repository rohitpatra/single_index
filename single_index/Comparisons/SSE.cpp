//
//  ComputeSSE
//  Simple score estimator for the single index model
//
//  Created by Piet Groeneboom on 04-05-18.
//  Copyright (c) 2018 Piet. All rights reserved.
//

#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include <Rcpp.h>

#define SQR(x) ((x)*(x))

using namespace std;
using namespace Rcpp;

#define SQR(x) ((x)*(x))

typedef struct
{
    int index;
    double v;
    double y;
}
data_object;

int         n,nIterations;
double      **xx,*yy,*yy1,*vv,*f,*cumw,*cs;
double      *psi,*derivative,mu,lambda,cc;

double      delta_tol,eta_tol;
double      eta0,mu0,omega0;
double      tau,gamma1,delta1,eta1;
double      alpha_omega,beta_omega,alpha_eta,beta_eta;
double      alpha_par,omega,eta,delta;

double  criterion(int m, double alpha[]);
void    sort_alpha(int m, int n, double **xx, double alpha[], double vv[], double yy[]);
void    convexmin(int n, double cumw[], double cs[], double y[]);
int     CompareTime(const void *a, const void *b);
double  best_nearby (int m, double delta[], double point[], double prevbest,
                     double f(int m, double alpha[]), int *funevals);
int     hooke(int m, double startpt[], double endpt[], double rho, double eps,
              int itermax, double f(int m, double alpha[]));
void    swap(double *x,double *y);
void    initialize();


// [[Rcpp::export]]

List ComputeSSE(NumericMatrix X, NumericVector y, NumericVector alpha0, int m1)
{
    int     i,j,m,iter;
    double  *alpha,*alpha_init;
    double  eps,rho,sum;
    
    // determine the sample size
    
    n = (int)(y.size());
    
    // m is the dimension
    
    m= (int)m1;
    
    rho=0.5;
    eps=1.0e-1;
    
    nIterations=100;
    // copy the data vector for use of the C++ procedures
    
    yy = new double[n];
    yy1 = new double[n+1];
    
    for (i=0;i<n;i++)
        yy[i]=(double)y[i];
    
    xx = new double *[n];
    for (i=0;i<n;i++)
        xx[i] = new double [m];
    
    for (i=0;i<n;i++)
    {
        for (j=0;j<m;j++)
          xx[i][j]=(double)X(i,j);
    }
    
    vv= new double[n];
    alpha= new double[m];
    alpha_init= new double[m];
    f= new double[m];
    
    for (i=0;i<m;i++)
        alpha_init[i]=(double)alpha0(i);
    
    cumw = new double[n+1];
    cs = new double[n+1];
    psi  = new double[n];
    
    cumw[0]=cs[0]=0;
    
    lambda=0;
    
    initialize();
    
    while (eta>eta_tol || delta>delta_tol)
    {
        iter = hooke(m,alpha_init,alpha,rho,delta,nIterations,criterion);
        
        sum=0;
        for (i=0;i<m;i++)
            sum += SQR(alpha[i]);
        
        sum -=1;
        
        if (fabs(sum)<eta)
        {
            if (eta<=eta_tol && delta<=delta_tol)
                break;
            lambda += sum/mu;
            alpha_par=fmin(mu,gamma1);
            omega=omega*pow(alpha_par,beta_omega);
            delta=omega/(1+lambda+1.0/mu);
            eta=eta*pow(alpha_par,alpha_eta);
        }
        else
        {
            mu=tau*mu;
            alpha_par=fmin(mu,gamma1);
            omega=omega0*pow(alpha_par,alpha_omega);
            delta=omega/(1+lambda+1.0/mu);
            eta=eta0*pow(alpha_par,alpha_eta);
        }
        
        for (j=0;j<m;j++)
            alpha_init[j]=alpha[j];
    }
    
    sum=0;
    
    for (i=0;i<m;i++)
        sum += SQR(alpha[i]);
    
    for (i=0;i<m;i++)
        alpha[i]/=sqrt(sum);

    NumericMatrix out0 = NumericMatrix(n,2);
    
    for (i=0;i<n;i++)
    {
        out0(i,0)=vv[i];
        out0(i,1)=yy[i];
    }
    
    
    NumericVector out1 = NumericVector(m);
    
    // computation of alpha
    
    for (i=0;i<m;i++)
        out1(i)=alpha[i];
    
    NumericMatrix out2 = NumericMatrix(n,2);
    
    for (i=0;i<n;i++)
    {
        out2(i,0)=vv[i];
        out2(i,1)=psi[i];
    }
    
     //Number of iterations of Nelder-Mead
    
    int out3 = iter;

    // make the list for the output, containing alpha and the estimate of psi
    
    List out = List::create(Rcpp::Named("data")=out0,Rcpp::Named("alpha")=out1,Rcpp::Named("psi")=out2,Rcpp::Named("niter")=out3);
    
    // free memory
   
    for (i=0;i<n;i++)
        delete[] xx[i];
    
    delete[] xx;
    
    delete[] yy; delete[] yy1; delete[] vv; delete[] alpha; delete[] alpha_init; delete[] f;
    delete[] cumw; delete[] cs; delete[] psi;
    
    return out;
}


void initialize()
{
    tau=gamma1=0.5;
    eta0=mu0=omega0=1;
    delta_tol=eta_tol=1.0e-10;
    alpha_omega=beta_omega=alpha_eta=beta_eta=1;
    
    lambda=0;
    mu=mu0;
    alpha_par=fmin(mu0,gamma1);
    omega=omega0*pow(alpha_par,alpha_omega);
    delta=omega/(1+lambda+1.0/mu);
    eta=eta0*pow(alpha_par,alpha_eta);
}

void convexmin(int n, double cumw[], double cs[], double y[])
{
  int    i, j, m;

  y[1] = cs[1]/cumw[1];
  for (i=2;i<=n;i++)
  {
    y[i] = (cs[i]-cs[i-1])/(cumw[i]-cumw[i-1]);
    if (y[i-1]>y[i])
    {
      j = i;
      while (y[j-1] > y[i] && j>1)
      {
        j--;
        if (j>1)
          y[i] = (cs[i]-cs[j-1])/(cumw[i]-cumw[j-1]);
        else
          y[i] = cs[i]/cumw[i];
        for (m=j;m<i;m++)    y[m] = y[i];
      }
    }
  }
}


double criterion(int m, double alpha[])
{
    int i,j;
    double sum,sum1;
    
    sort_alpha(m,n,xx,alpha,vv,yy);
    
    yy1[0]=0;
    
    for (i=1;i<=n;i++)
        yy1[i]=yy[i-1];
    
    for (i=1;i<=n;i++)
    {
        cumw[i]=i*1.0;
        cs[i]=cs[i-1]+yy1[i];
    }
    
    convexmin(n,cumw,cs,yy1);
    
    for (i=0;i<n;i++)
        psi[i]=yy1[i+1];
    
    for (j=0;j<m;j++)
        f[j]=0;
    
    for (j=0;j<m;j++)
    {
        for (i=0;i<n;i++)
            f[j] += xx[i][j]*(psi[i]-yy[i]);
    }
    
    sum1=0;
    
    for (i=0;i<m;i++)
        sum1 += SQR(alpha[i]);
    
    sum1 -=1;
    
    sum=0;
    
    for (i=0;i<m;i++)
        sum += SQR(f[i]);
    
    sum += lambda*sum1+SQR(sum1)/mu;
    
    return sum;
    
}

int CompareTime(const void *a, const void *b)
{
    if ((*(data_object *) a).v < (*(data_object *) b).v)
        return -1;
    if ((*(data_object *) a).v > (*(data_object *) b).v)
        return 1;
    return 0;
}

void sort_alpha(int m, int n, double **xx, double alpha[], double vv[], double yy[])
{
    int i,j,*ind;
    double **xx_new;
    data_object *obs;
    
    obs= new data_object[n];
    ind= new int[n];
    
    xx_new = new double *[n];
    for (i=0;i<n;i++)
        xx_new[i] = new double [m];
    
    for (i=0;i<n;i++)
    {
        vv[i]=0;
        for (j=0;j<m;j++)
            vv[i] += alpha[j]*xx[i][j];
    }
    
    for (i=0;i<n;i++)
    {
        obs[i].index=i;
        obs[i].v=vv[i];
        obs[i].y=yy[i];
    }
    
    qsort(obs,n,sizeof(data_object),CompareTime);
    
    for (i=0;i<n;i++)
        ind[i]=obs[i].index;
    
    
    for (i=0;i<n;i++)
        for (j=0;j<m;j++)
            xx_new[i][j]=xx[ind[i]][j];
    
    for (i=0;i<n;i++)
    {
        for (j=0;j<m;j++)
            xx[i][j]=xx_new[i][j];
        vv[i]=obs[i].v;
        yy[i]=obs[i].y;
    }
    
    delete[] obs;
    
    delete[] ind;
    for (i=0;i<n;i++)
        delete[] xx_new[i];
    delete[] xx_new;
}

double best_nearby (int m, double delta[], double point[], double prevbest,
                    double f(int m, double alpha[]), int *funevals)
{
    double ftmp,minf,*z;
    int i;
    
    z = new double[m];
    
    minf = prevbest;
    
    for ( i = 0; i < m; i++ )
        z[i] = point[i];
    
    for ( i = 0; i < m; i++ )
    {
        z[i] = point[i] + delta[i];
        
        ftmp = f(m,z);
        *funevals = *funevals + 1;
        
        if ( ftmp < minf )
            minf = ftmp;
        else
        {
            delta[i] = - delta[i];
            z[i] = point[i] + delta[i];
            ftmp = f(m,z);
            *funevals = *funevals + 1;
            
            if ( ftmp < minf )
                minf = ftmp;
            else
                z[i] = point[i];
        }
    }
    
    for ( i = 0; i < m; i++ )
        point[i] = z[i];
    
    delete [] z;
    
    return minf;
}

int hooke(int m, double startpt[], double endpt[], double rho, double eps,
          int itermax, double f(int m, double alpha[]))
{
    double *delta,fbefore;
    int i,iters,keep,funevals,count;
    double newf,*newx,steplength,tmp;
    bool verbose = false;
    double *xbefore;
    
    delta = new double[m];
    newx = new double[m];
    xbefore = new double[m];
    
    for ( i = 0; i < m; i++ )
        xbefore[i] = newx[i] = startpt[i];
    
    for ( i = 0; i < m; i++ )
    {
        if ( startpt[i] == 0.0 )
            delta[i] = rho;
        else
            delta[i] = rho*fabs(startpt[i]);
    }
    
    funevals = 0;
    steplength = rho;
    iters = 0;
    
    
    fbefore = f(m,newx);
    funevals = funevals + 1;
    newf = fbefore;
    
    while ( iters < itermax && eps < steplength )
    {
        iters = iters + 1;
        
        if (verbose)
        {
            cout << "\n";
            cout << "  FUNEVALS, = " << funevals
            << "  F(X) = " << fbefore << "\n";
            
            for ( i = 0; i < m; i++ )
            {
                cout << "  " << i + 1
                << "  " << xbefore[i] << "\n";
            }
        }
        //
        //  Find best new alpha, one coordinate at a time.
        //
        for ( i = 0; i < m; i++ )
            newx[i] = xbefore[i];
        
        
        
        newf = best_nearby(m,delta,newx,fbefore,f,&funevals);
        //
        //  If we made some improvements, pursue that direction.
        //
        keep = 1;
        count=0;
        
        while (newf<fbefore && keep == 1 && count<=100)
        {
            count++;
            for ( i = 0; i < m; i++ )
            {
                //
                //  Arrange the sign of DELTA.
                //
                if ( newx[i] <= xbefore[i] )
                    delta[i] = - fabs(delta[i]);
                else
                    delta[i] = fabs(delta[i]);
                //
                //  Now, move further in this direction.
                //
                tmp = xbefore[i];
                xbefore[i] = newx[i];
                newx[i] = newx[i] + newx[i] - tmp;
            }
            
            fbefore = newf;
            
            newf = best_nearby(m,delta,newx,fbefore,f,&funevals);
            //
            //  If the further (optimistic) move was bad...
            //
            if (fbefore <= newf)
                break;
            //
            //  Make sure that the differences between the new and the old points
            //  are due to actual displacements; beware of roundoff errors that
            //  might cause NEWF < FBEFORE.
            //
            keep = 0;
            
            for ( i = 0; i < m; i++ )
            {
                if ( 0.5 * fabs(delta[i]) < fabs(newx[i]-xbefore[i]))
                {
                    keep = 1;
                    break;
                }
            }
        }
        
        if (eps <= steplength && fbefore <= newf)
        {
            steplength = steplength * rho;
            for ( i = 0; i < m; i++ )
                delta[i] = delta[i] * rho;
        }
        
    }
    
    for ( i = 0; i < m; i++ )
        endpt[i] = xbefore[i];
    
    delete [] delta;
    delete [] newx;
    delete [] xbefore;
    
    return iters;
}


void swap(double *x,double *y)
{
    double temp;
    temp=*x;
    *x=*y;
    *y=temp;
}


