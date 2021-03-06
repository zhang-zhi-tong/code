/* This code accompanies
 *   The Lattice Boltzmann Method: Principles and Practice
 *   T. Krüger, H. Kusumaatmaja, A. Kuzmin, O. Shardt, G. Silva, E.M. Viggen
 *   ISBN 978-3-319-44649-3 (Electronic) 
 *        978-3-319-44647-9 (Print)
 *   http://www.springer.com/978-3-319-44647-9
 *
 * This code is provided under the MIT license. See LICENSE.txt.
 *
 * Author: Alexandr Kuzmin
 * 
 */
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <vector>

//Domain size
int NY;
int NX;
int NUM;

//Other constants
const int NPOP=9;

//Time steps
int N=50000;
int NOUTPUT=100;

//Fields and populations
double *f;
double *f2;
double *rho;
double *ux;
double *uy;
int * geometry;

//Boundary conditions
double conc_wall=1.0;
double conc_inlet=0.0;
double u0=0.05;
std::vector<int> bb_nodes_inlet;
std::vector<char>* dirs_inlet;

std::vector<int> bb_nodes_wall;
std::vector<int> dirs_wall;

//BGK relaxation parameter
double omega=1.0/(4.0*(1.0/1.4-0.5)+0.5);
double omega_plus=2.0-omega;
double omega_minus=omega;

//Diffusion parameter
double ce=1.0/3.0;

//Underlying lattice parameters
double weights[]={4.0/9.0,1.0/9.0,1.0/9.0,1.0/9.0,1.0/9.0,1.0/36.0,1.0/36.0,1.0/36.0,1.0/36.0};
double weights_trt[]={0.0,1.0/3.0,1.0/3.0,1.0/3.0,1.0/3.0,1.0/12.0,1.0/12.0,1.0/12.0,1.0/12.0};
int cx[]={0,1,0,-1,0,1,-1,-1,1};
int cy[]={0,0,1,0,-1,1,1,-1,-1};
int complement[]={0,3,4,1,2,7,8,5,6};
int pxx[]={0, 1, -1, 1, -1, 0, 0, 0, 0};
int pxy[]={0, 0, 0, 0, 0, 1, -1, 1, -1};
void writedensity(std::string const & fname)
{
    std::string filename = "film_antibb/" + fname + ".dat";
    std::ofstream fout(filename.c_str());
    fout.precision(10);

    for (int iX=0; iX<NX; ++iX)
    {
        for (int iY=0; iY<NY; iY++)
        {
            int counter=iY*NX+iX;
            fout<<rho[counter]<<" ";
        }
        fout<<"\n";
    }
}

void writevelocityx(std::string const & fname)
{
    std::string filename = "film_antibb/" + fname+".dat";
    std::ofstream fout(filename.c_str());
    fout.precision(10);

    for (int iX=0; iX<NX; ++iX)
    {
        for (int iY=0; iY<NY; iY++)
        {
            int counter=iY*NX+iX;
            fout<<ux[counter]<<" ";
        }
        fout<<"\n";
    }
}

void writevelocityy(std::string const & fname)
{
    std::string filename = "film_antibb/" + fname + ".dat";
    std::ofstream fout(filename.c_str());
    fout.precision(10);

    for (int iX=0; iX<NX; ++iX)
    {
        for (int iY=0; iY<NY; iY++)
        {
            int counter=iY*NX+iX;
            fout<<uy[counter]<<" ";
        }
        fout<<"\n";
    }
}

void collide()
{
    for (int iY=0;iY<NY;iY++)
        for(int iX=0;iX<NX;iX++)
        {
            int counter=iY*NX+iX;
            rho[counter]=0.0;
            
            int offset=counter*NPOP;
     
            double sum=0;
            for(int k=0;k<NPOP;k++)
                sum+=f[offset+k];
    
            rho[counter]=sum;

            double dense_temp=rho[counter];
            double ux_temp=ux[counter];
            double uy_temp=uy[counter];
            
            double feq[NPOP];
     
            //Collision operator
            for(int k=0; k < NPOP; k++)
            {
                feq[k]=weights[k]*dense_temp*(1.0+3.0*(cx[k]*ux_temp+cy[k]*uy_temp)+4.5*((cx[k]*cx[k]-1.0/3.0)*ux_temp*ux_temp+2.0*cx[k]*cy[k]*ux_temp*uy_temp+(cy[k]*cy[k]-1.0/3.0)*uy_temp*uy_temp));
                f2[offset+k]=f[offset+k]-omega*(f[offset+k]-feq[k]);
            }
    }
            
}


void update_bounce_back()
{

    // Updating inlet populations
    for(int iX = 1; iX < NX - 1; iX++)
    {
        int offset=iX*NPOP;

        f2[offset + 2] = - f2[(NX + iX) * NPOP + 4] + 2 * weights[2] * conc_inlet;
        f2[offset + 5] = - f2[(NX + iX + 1) * NPOP + 7] + 2 * weights[5] * conc_inlet;
        f2[offset + 6] = - f2[(NX + iX - 1) * NPOP + 8] + 2 * weights[6] * conc_inlet;
    
    }

    // Updating wall populations
    for(int iY = 0; iY < NY; iY++)
    {

        int ytop = (iY + 1 + NY) % NY;
        int ybottom = (iY - 1 + NY) % NY;
        int offset = (iY * NX + NX - 1) * NPOP;

        f2[offset + 3] = - f2[(iY * NX + NX - 2) * NPOP + 1] + 2 * weights[3] * conc_wall;
        f2[offset + 6] = - f2[(ytop * NX +  NX - 2) * NPOP + 8] + 2 * weights[6] * conc_wall;
        f2[offset + 7] = - f2[(ybottom * NX + NX - 2) * NPOP + 5] + 2 * weights[7] * conc_wall;

        rho[iY * NX + NX - 1] = conc_wall;
    }


    // Updating outlet populations
    for(int iX = 1; iX < NX-1; iX++)
    {
        int offset = ((NY-1) * NX + iX) * NPOP;
        int offset2 = ((NY - 2) * NX + iX) * NPOP;

        for (int k = 0; k < NPOP; k++)
            f2[offset + k] = f2[offset2 + k];
    }

    // Updating the free wall
    for (int iY = 0; iY < NY; ++iY)
    {

        int offset = iY * NX * NPOP;
        int ytop = (iY + 1 + NY) % NY;
        int ybottom = (iY - 1 + NY) % NY;

        f2[offset + 1] = f2[(iY * NX + 1) * NPOP + 3];
        f2[offset + 5] = f2[(ytop * NX + 1) * NPOP + 7];
        f2[offset + 8] = f2[(ybottom * NX + 1) * NPOP + 6];
        
        rho[iY*NX] = rho[iY*NX + 1];
    }

    // Uncomment below if you want to have simplest update
    /*for(int iY = 0; iY < NY; iY++)
    {
        int offset = (iY * NX + 0) * NPOP;
        int offset2 = (iY * NX + 1) * NPOP;

        for (int k = 0; k < NPOP; k++)
            f2[offset + k] = f2[offset2 + k];
    }*/
            
}

void initialize_geometry()
{
    NY=10*160;
    NX=160;
    NUM=NX*NY;
    geometry=new int[NUM];
    rho=new double[NUM];
    ux=new double[NUM];
    uy=new double[NUM];
    
 
    for(int iY=0;iY<NY;iY++)
        for(int iX=0;iX<NX;iX++)
        {
            int counter=iY*NX+iX;
            rho[counter]=0.0;
            uy[counter]=u0*(1.0-double((iX-0.5)*(iX-0.5)/((NX-2)*(NX-2))));
            ux[counter]=0.0;
        }
 
    for(int iX=1;iX<NX-1;iX++)
    { 
        rho[iX]=conc_inlet;
    }
    
    for(int iY=0;iY<NY;iY++)
    {
        rho[iY*NX + NX - 1] = conc_wall;
    }

    writedensity("conc_initial");
    writevelocityx("ux_initial");
    writevelocityy("uy_initial");    
}

void init()
{
    //Creating arrays
    f=new double[NUM*NPOP];
    f2=new double[NUM*NPOP];
    
    //Bulk nodes initialization
    double feq;
    
    for(int iY=0;iY<NY;iY++)
        for(int iX=0;iX<NX;iX++)
        {
            int  counter=iY*NX+iX;
            double dense_temp=rho[counter];
            double ux_temp=ux[counter];
            double uy_temp=uy[counter];
            
            for (int k=0; k<NPOP; k++)
            {
                feq=weights[k]*(dense_temp+3.0*dense_temp*(cx[k]*ux_temp+cy[k]*uy_temp)
                                +4.5*dense_temp*((cx[k]*cx[k]-1.0/3.0)*ux_temp*ux_temp
                                                +(cy[k]*cy[k]-1.0/3.0)*uy_temp*uy_temp
                                                +2.0*ux_temp*uy_temp*cx[k]*cy[k]));
                f[counter*NPOP+k]=feq;
            }
            
        }

}



void finish_simulation()
{
    delete[] geometry;
    delete[] rho;
    delete[] ux;
    delete[] uy;
    delete[] f;
    delete[] f2;
}


void stream()
{
    for (int iY=0;iY<NY;iY++)
        for(int iX=0;iX<NX;iX++)
        {
            int counter=iY*NX+iX;
            for(int iPop=0;iPop<NPOP;iPop++)
            {
                int iX2=(iX+cx[iPop]+NX)%NX;
                int iY2=(iY+cy[iPop]+NY)%NY;
                int counter2=iY2*NX+iX2;
                f[counter2*NPOP+iPop]=f2[counter*NPOP+iPop];
            }
        }
}


int main(int argc, char* argv[])
{
    initialize_geometry();
    
    init();

    for(int counter=0;counter<=N;counter++)
    {

        collide();
        update_bounce_back();
        stream();
        
        //Writing files
        if (counter%NOUTPUT==0)
        {
            std::cout<<"Counter="<<counter<<"\n";
            std::stringstream filewritedensity;
             
            std::stringstream counterconvert;
            counterconvert<<counter;
            filewritedensity<<std::fixed;

            filewritedensity<<"film_antibb"<<std::string(7-counterconvert.str().size(),'0')<<counter;
            
            writedensity(filewritedensity.str());
        }

    }

    finish_simulation();
    
    return 0;
}
