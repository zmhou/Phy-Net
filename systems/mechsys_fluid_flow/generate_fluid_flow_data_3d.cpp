/************************************************************************
 * MechSys - Open Library for Mechanical Systems                        *
 * Copyright (C) 2009 Sergio Galindo                                    *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * any later version.                                                   *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program. If not, see <http://www.gnu.org/licenses/>  *
 ************************************************************************/

// FLow in a pipe with obstacle


// MechSys
#include <mechsys/flbm/Domain.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

#include <iostream>
#include <string>


struct UserData
{
    double      * Vel;
    double        vmax;
    double        rho;
    #ifdef USE_OCL
    cl::Buffer        bBCVel;
    cl::Program       UserProgram;
    #endif
};

void Setup (FLBM::Domain & dom, void * UD)
{
    UserData & dat = (*static_cast<UserData *>(UD));
    
    #ifdef USE_OCL
    if (dom.IsFirstTime)
    {
        dom.IsFirstTime = false;
        dat.bBCVel      = cl::Buffer(dom.CL_Context,CL_MEM_READ_WRITE,sizeof(double)*dom.Ndim[1]*dom.Ndim[2]);
        dom.CL_Queue.enqueueWriteBuffer(dat.bBCVel,CL_TRUE,0,sizeof(double)*dom.Ndim[1]*dom.Ndim[2],dat.Vel);
        
        char* pMECHSYS_ROOT;
        pMECHSYS_ROOT = getenv ("MECHSYS_ROOT");
        if (pMECHSYS_ROOT==NULL) pMECHSYS_ROOT = getenv ("HOME");

        String pCL;
        pCL.Printf("%s/mechsys/lib/flbm/lbm.cl",pMECHSYS_ROOT);

        std::ifstream infile(pCL.CStr(),std::ifstream::in);
        std::string main_kernel_code((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
        
        std::string BC_kernel_code =
            " void kernel Left_BC (global double * VBC, global const bool * IsSolid, global double * F, global double3 * Vel, global double * Rho, global const struct lbm_aux * lbmaux) \n"
            " { \n"
                " size_t ic  = get_global_id(0); \n"
                " size_t ib  = ic*lbmaux[0].Nx; \n"
                " if (!IsSolid[ib]) \n"
                " { \n"
                    " Initialize(ib,F,Rho,Vel,1.0,(double3)(VBC[ic],0.0,0.0),lbmaux); \n"
                    //" size_t iv  = ib*lbmaux[0].Nneigh; \n"
		            //" double rho = (F[iv+0]+F[iv+2]+F[iv+4] + 2.0*(F[iv+3]+F[iv+6]+F[iv+7]))/(1.0-VBC[ic]); \n"
		            //" F[iv+1] = F[iv+3] + (2.0/3.0)*rho*VBC[ic]; \n"
		            //" F[iv+5] = F[iv+7] + (1.0/6.0)*rho*VBC[ic] - 0.5*(F[iv+2]-F[iv+4]); \n"
		            //" F[iv+8] = F[iv+6] + (1.0/6.0)*rho*VBC[ic] + 0.5*(F[iv+2]-F[iv+4]); \n"
                    //" Rho   [ib] = 0.0; \n"
                    //" Vel   [ib] = (double3)(0.0,0.0,0.0); \n"
                    //" for(size_t k=0;k<lbmaux[0].Nneigh;k++) \n"
                    //" { \n"
                        //" Rho[ib] += F[iv + k]; \n"
                        //" Vel[ib] += F[iv + k]*lbmaux[0].C[k]; \n"
                    //" } \n"
                    //" Vel[ib] /= Rho[ib]; \n"
                " } \n"
            " } \n"
            
            " void kernel Right_BC (global const bool * IsSolid, global double * F, global double3 * Vel, global double * Rho, global const struct lbm_aux * lbmaux) \n"
            " { \n"
                " size_t ic  = get_global_id(0); \n"
                " size_t ib  = ic*lbmaux[0].Nx + lbmaux[0].Nx-1; \n"
                " if (!IsSolid[ib]) \n"
                " { \n"
                    " Initialize(ib,F,Rho,Vel,1.0,Vel[ib],lbmaux); \n"
                    //" size_t iv  = ib*lbmaux[0].Nneigh; \n"
                    //" double rho = 1.0; \n"
		            //" double vx = -1.0 + (F[iv+0]+F[iv+2]+F[iv+4] + 2.0*(F[iv+1]+F[iv+5]+F[iv+8]))/rho; \n"
		            //" F[iv+3] = F[iv+1] - (2.0/3.0)*rho*vx;  \n"
		            //" F[iv+7] = F[iv+5] - (1.0/6.0)*rho*vx + 0.5*(F[iv+2]-F[iv+4]); \n"
		            //" F[iv+6] = F[iv+8] - (1.0/6.0)*rho*vx - 0.5*(F[iv+2]-F[iv+4]); \n"
                    //" Rho   [ib] = 0.0; \n"
                    //" Vel   [ib] = (double3)(0.0,0.0,0.0); \n"
                    //" for(size_t k=0;k<lbmaux[0].Nneigh;k++) \n"
                    //" { \n"
                        //" Rho[ib] += F[iv + k]; \n"
                        //" Vel[ib] += F[iv + k]*lbmaux[0].C[k]; \n"
                    //" } \n"
                    //" Vel[ib] /= Rho[ib]; \n"
                " } \n"
           " } \n"
        ;

        BC_kernel_code = main_kernel_code + BC_kernel_code;

        cl::Program::Sources sources;
        sources.push_back({BC_kernel_code.c_str(),BC_kernel_code.length()});

        dat.UserProgram = cl::Program(dom.CL_Context,sources);
        if(dat.UserProgram.build({dom.CL_Device})!=CL_SUCCESS){
            std::cout<<" Error building: "<<dat.UserProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dom.CL_Device)<<"\n";
            exit(1);
        }

    }

    cl::Kernel kernel(dat.UserProgram,"Left_BC");
    kernel.setArg(0,dat.bBCVel     );
    kernel.setArg(1,dom.bIsSolid[0]);
    kernel.setArg(2,dom.bF      [0]);
    kernel.setArg(3,dom.bVel    [0]);
    kernel.setArg(4,dom.bRho    [0]);
    kernel.setArg(5,dom.blbmaux    );
    dom.CL_Queue.enqueueNDRangeKernel(kernel,cl::NullRange,cl::NDRange(dom.Ndim[1]*dom.Ndim[2]),cl::NullRange);
    dom.CL_Queue.finish();

    //kernel = cl::Kernel(dat.UserProgram,"Right_BC");
    //kernel.setArg(0,dom.bIsSolid[0]);
    //kernel.setArg(1,dom.bF      [0]);
    //kernel.setArg(2,dom.bVel    [0]);
    //kernel.setArg(3,dom.bRho    [0]);
    //kernel.setArg(4,dom.blbmaux    );
    //dom.CL_Queue.enqueueNDRangeKernel(kernel,cl::NullRange,cl::NDRange(dom.Ndim[1]),cl::NullRange);
    //dom.CL_Queue.finish();


    #else // USE_OCL
    // Cells with prescribed velocity
    #ifdef USE_OMP
    #pragma omp parallel for schedule(static) num_threads(dom.Nproc)
    #endif
	for (size_t i=0; i<dom.Ndim(1); ++i)
	for (size_t j=0; j<dom.Ndim(2); ++j)
	{
        double * f = dom.F[0][0][i][j];
		double rho = (f[0]+f[2]+f[4] + 2.0*(f[3]+f[6]+f[7]))/(1.0-dat.Vel[i + j*dom.Ndim(1)]);
		f[1] = f[3] + (2.0/3.0)*rho*dat.Vel[i + j*dom.Ndim(1)];
		f[5] = f[7] + (1.0/6.0)*rho*dat.Vel[i + j*dom.Ndim(1)] - 0.5*(f[2]-f[4]);
		f[8] = f[6] + (1.0/6.0)*rho*dat.Vel[i + j*dom.Ndim(1)] + 0.5*(f[2]-f[4]);
        dom.Vel[0][0][i][j] = OrthoSys::O;
        dom.Rho[0][0][i][j] = 0.0;
        for (size_t k=0;k<dom.Nneigh;k++)
        {
            dom.Rho[0][0][i][j] +=  dom.F[0][0][i][j][k];
            dom.Vel[0][0][i][j] +=  dom.F[0][0][i][j][k]*dom.C[k];
        }
        dom.Vel[0][0][i][j] /= dom.Rho[0][0][i][j];
	}

	// Cells with prescribed density
    #ifdef USE_OMP
    #pragma omp parallel for schedule(static) num_threads(dom.Nproc)
    #endif
	for (size_t i=0; i<dom.Ndim(1); ++i)
	for (size_t j=0; j<dom.Ndim(2); ++j)
	{
        double * f = dom.F[0][dom.Ndim(0)-1][i][j];
		double vx = -1.0 + (f[0]+f[2]+f[4] + 2.0*(f[1]+f[5]+f[8]))/dat.rho;
		f[3] = f[1] - (2.0/3.0)*dat.rho*vx; 
		f[7] = f[5] - (1.0/6.0)*dat.rho*vx + 0.5*(f[2]-f[4]);
		f[6] = f[8] - (1.0/6.0)*dat.rho*vx - 0.5*(f[2]-f[4]);
        dom.Vel[0][dom.Ndim(0)-1][i][j] = OrthoSys::O;
        dom.Rho[0][dom.Ndim(0)-1][i][j] = 0.0;
        for (size_t k=0;k<dom.Nneigh;k++)
        {
            dom.Rho[0][dom.Ndim(0)-1][i][j] +=  dom.F[0][dom.Ndim(0)-1][i][j][k];
            dom.Vel[0][dom.Ndim(0)-1][i][j] +=  dom.F[0][dom.Ndim(0)-1][i][j][k]*dom.C[k];
        }
        dom.Vel[0][dom.Ndim(0)-1][i][j] /= dom.Rho[0][dom.Ndim(0)-1][i][j];
	}
    #endif // USE_OCL
}

int in_sphere(double x, double y, double z, double x_pos, double y_pos, double z_pos, double x_r, double y_r, double z_r, double alpha, double beta)
{
    double term_x = (pow(cos(beta)*(x-x_pos) - sin(beta)*(z-z_pos),2.0))/(x_r*x_r);
    double term_y = (pow(sin(alpha)*sin(beta)*(x-x_pos) + cos(alpha)*(y-y_pos) + sin(alpha)*cos(beta)*(z-z_pos),2.0))/(y_r*y_r);
    double term_z = (pow(cos(alpha)*sin(beta)*(x-x_pos) - sin(alpha)*(y-y_pos) + cos(alpha)*cos(beta)*(z-z_pos),2.0))/(y_r*y_r);
    int in_circle = 0;
    if ((term_x + term_y + term_z) < 1.0)
    {
        in_circle = 1;
    }
    return in_circle;
}

int main(int argc, char **argv) try
{
    // seed rand
    srand(time(NULL));

    // need params to run
    if (argc!=3) {
        printf("need to give both the dimention to run and filename to save too \n");
        exit(1);
    }
    size_t Nproc = 1; 
    double u_max  = 0.1;                // Poiseuille's maximum velocity
    double Re     = 3000.0;                  // Reynold's number
    size_t nx = atoi(argv[1]);
    size_t ny = nx/4;
    size_t nz = nx/4;
    double nu     = u_max*(2*12)/Re; // viscocity (hard set now)
    FLBM::Domain Dom(D3Q15, nu, iVec3_t(nx,ny,nz), 1.0, 1.0);
    
    UserData dat;
    Dom.UserData = &dat;

    dat.vmax = u_max;

    dat.Vel = new double[ny*nz]; 
    for (size_t i=0;i<ny*nz;i++)
    {
        //double vx = dat.vmax*4/(L*L)*(L*yp - yp*yp); // horizontal velocity
        double vx = 0.04; // horizontal velocity
        dat.Vel[i] = vx;
    }
    
    dat.rho  = 1.0;

    // number of objects between 10 and 25
    //size_t num_objects = (rand() % 10) + 10;
    size_t num_objects = 3*(nx/160)*(nx/160)*(nx/160);

    // set objects
    size_t h = 0;
    size_t trys = 0;
    while (h<num_objects && trys<1000)
    {
        trys++;
        int object_type = (rand() % 2);
        if (object_type == 0) // oval
        {
	    // set inner obstacle
            int radius_x = 12;
            int radius_y = 12;
            int radius_z = 12;
            int max_radius = radius_x; 
            if (radius_y > radius_x) { max_radius = radius_y; }
            if (radius_z > max_radius) { max_radius = radius_z; }
	    double obsX   = (rand() % (nx-(3*max_radius))) + (1.5*max_radius) ;   // x position
	    double obsY   = (rand() % (ny-(2*max_radius))) + (max_radius) ;   // y position
	    double obsZ   = (rand() % (nz-(2*max_radius))) + (max_radius) ;   // z position
            int alpha = (rand() % 90);
            int beta = (rand() % 90);
            int place_object = 1; 
            for (size_t i=0;i<nx;i++)
            for (size_t j=0;j<ny+2*max_radius;j++)
            for (size_t k=0;k<nz+2*max_radius;k++)
            {
                if (in_sphere(double(i),double(j),double(k),obsX,obsY,obsZ,1.0*radius_x,1.0*radius_y,1.0*radius_z,alpha,beta) == 1)
                {
                    int j_ind = j-max_radius;
                    int k_ind = k-max_radius;
                    if (j_ind < 0)    { j_ind = ny + j_ind; }
                    if (j_ind > ny-1) { j_ind = j_ind - ny; }
                    if (k_ind < 0)    { k_ind = nz + k_ind; }
                    if (k_ind > nz-1) { k_ind = k_ind - nz; }
                    if (Dom.IsSolid[0][i][j_ind][k_ind])
                    {
                        place_object = 0;
                    }
                }
            }
            if (place_object == 1)
            {
                h++;
                for (size_t i=0;i<nx;i++)
                for (size_t j=0;j<ny+2*max_radius;j++)
                for (size_t k=0;k<nz+2*max_radius;k++)
                {

                    if (in_sphere(double(i),double(j),double(k),obsX,obsY,obsZ,radius_x,radius_y,radius_z,alpha,beta) == 1)
                    {
                        int j_ind = j-max_radius;
                        int k_ind = k-max_radius;
                        if (j_ind < 0)    { j_ind = ny + j_ind; }
                        if (j_ind > ny-1) { j_ind = j_ind - ny; }
                        if (k_ind < 0)    { k_ind = nz + k_ind; }
                        if (k_ind > nz-1) { k_ind = k_ind - nz; }
                        Dom.IsSolid[0][i][j_ind][k_ind] = true;
                    }
                }
            }

        }


    }

    //Assigning solid boundaries at top and bottom (off for now)
    for (size_t i=0;i<nx;i++)
    {
        //Dom.IsSolid[0][i][0][0]    = true;
        //Dom.IsSolid[0][i][ny-1][0] = true;
    }


    double rho0 = 1.0;
    Vec3_t v0(0.04,0.0,0.0);

    for (size_t ix=0;ix<nx;ix++)
    for (size_t iy=0;iy<ny;iy++)
    for (size_t iz=0;iz<nz;iz++)
    {
        iVec3_t idx(ix,iy,iz);
        Dom.Initialize(0,idx,rho0,v0);
    }  
     
    Dom.Solve(15000.0,60.0,Setup,NULL,argv[2],true,Nproc);


}
MECHSYS_CATCH
