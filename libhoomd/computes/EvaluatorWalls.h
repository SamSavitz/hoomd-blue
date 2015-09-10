/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2009-2015 The Regents of
the University of Michigan All rights reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

You may redistribute, use, and create derivate works of HOOMD-blue, in source
and binary forms, provided you abide by the following conditions:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer both in the code and
prominently in any materials provided with the distribution.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* All publications and presentations based on HOOMD-blue, including any reports
or published results obtained, in whole or in part, with HOOMD-blue, will
acknowledge its use according to the terms posted at the time of submission on:
http://codeblue.umich.edu/hoomd-blue/citations.html

* Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
http://codeblue.umich.edu/hoomd-blue/

* Apart from the above required attributions, neither the name of the copyright
holder nor the names of HOOMD-blue's contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Maintainer: jproc

/*! \file EvaluatorWalls.h
    \brief Executes an external field potential of several evaluator types for each wall in the system.
 */

#ifndef __EVALUATOR_WALLS_H__
#define __EVALUATOR_WALLS_H__

// need to declare these class methods with __device__ qualifiers when building in nvcc
// DEVICE is __host__ __device__ when included in nvcc and blank when included into the host compiler
#ifdef NVCC
#define DEVICE __device__
#else
#define DEVICE
#endif

#define MAX_N_SWALLS 20
#define MAX_N_CWALLS 20
#define MAX_N_PWALLS 40
 // take a look at what sam is doing in hpmc.
//
//
//

#include "HOOMDMath.h"
#include "VectorMath.h"
#include "WallData.h"



template<class evaluator>
class EvaluatorWalls
	{
	public:
        typedef struct{
        	typename evaluator::param_type params;
			Scalar rcutsq;
        	Scalar ronsq;
        } param_type;
		typedef struct _field_type{
			SphereWall m_Spheres[MAX_N_SWALLS];
			CylinderWall m_Cylinders[MAX_N_CWALLS];
			PlaneWall m_Planes[MAX_N_PWALLS];
			unsigned int numSpheres;
			unsigned int numCylinders;
			unsigned int numPlanes;
			// add a constructor
		} field_type;

		DEVICE EvaluatorWalls(Scalar3 pos, unsigned int i, const BoxDim& box, const param_type& p, const field_type& f) : m_pos(pos), m_box(box), idx(i), params(p)
			{
			//This runs once for every single particle... Probably should fix somehow
			field.m_Spheres[0].r = 15;
			field.m_Spheres[0].origin = vec3<Scalar>(0,0,0);
			field.m_Spheres[0].inside = true; //TODO:remove after python interface for walls is fixed
			field.numSpheres = 1;
			// field.m_Cylinders[0] = CylinderWall(2.0,vec3<Scalar>(0,0,0),vec3<Scalar>(0,0,1),true);
			field.numCylinders = 0;
			// field.m_Planes[0] = PlaneWall(vec3<Scalar>(0,0,1),vec3<Scalar>(0,0,-1));
			field.numPlanes = 0;
			}

		DEVICE inline vec3<Scalar> wall_eval_dist(const SphereWall& wall, const vec3<Scalar>& position, const BoxDim& box)
		    {
		    // vec3<Scalar> t = position - box_origin;
		    // box.minImage(t);
		    // t-=wall.origin;
		    // vec3<Scalar> shifted_pos(t);
		    // Scalar rxyz_sq = shifted_pos.x*shifted_pos.x + shifted_pos.y*shifted_pos.y + shifted_pos.z*shifted_pos.z;
		    // Scalar r = wall.r - sqrt(rxyz_sq);
			vec3<Scalar> t = position;
			t-=wall.origin;
			vec3<Scalar> shifted_pos(t);
			Scalar rxyz = sqrt(dot(shifted_pos,shifted_pos));
			if (((rxyz < wall.r) && wall.inside) || ((rxyz > wall.r) && !(wall.inside)))
				{
				t *= wall.r/rxyz;
				vec3<Scalar> dx = t - shifted_pos;
				return dx;
				}
			else
				{
				return vec3<Scalar>(0.0,0.0,0.0);
				}
		    };

		DEVICE inline vec3<Scalar> wall_eval_dist(const CylinderWall& wall, const vec3<Scalar>& position, const BoxDim& box)
		    {
		    // vec3<Scalar> t = position - box_origin;
		    // box.minImage(t);
		    // t-=wall.origin;
		    // vec3<Scalar> shifted_pos=rotate(wall.q_reorientation,t);
		    // Scalar rxy_sq= shifted_pos.x*shifted_pos.x + shifted_pos.y*shifted_pos.y;
		    // Scalar r = wall.r - sqrt(rxy_sq);
			vec3<Scalar> t = position;
	        box.minImage(t);
	        t-=wall.origin;
	        vec3<Scalar> shifted_pos = rotate(wall.q_reorientation,t);
			shifted_pos.z = 0;
	        Scalar rxy = sqrt(dot(shifted_pos,shifted_pos));
			if (((rxy < wall.r) && wall.inside) || ((rxy > wall.r) && !(wall.inside)))
				{
		        t = (wall.r / rxy) * shifted_pos;
		        vec3<Scalar> dx = t - shifted_pos;
				dx = rotate(conj(wall.q_reorientation),dx);
				return dx;
				}
			else
				{
				return vec3<Scalar>(0.0,0.0,0.0);
				}
		    };

		DEVICE inline vec3<Scalar> wall_eval_dist(const PlaneWall& wall, const vec3<Scalar>& position, const BoxDim& box)
		    {
		    // vec3<Scalar> t = position - box_origin;
		    // box.minImage(t);
		    // Scalar r =dot(wall.normal,t)-dot(wall.normal,wall.origin);
			vec3<Scalar> t = position;
			box.minImage(t);
			Scalar wall_dist = dot(wall.normal,t) - dot(wall.normal,wall.origin);
			if (wall_dist > 0.0)
				{
				vec3<Scalar> dx = wall_dist * wall.normal;
				return dx;
				}
			else
				{
				return vec3<Scalar>(0.0,0.0,0.0);
				}
			};

		DEVICE void evalForceEnergyAndVirial(Scalar3& F, Scalar& energy, Scalar* virial)
			{
			F.x = Scalar(0.0);
            F.y = Scalar(0.0);
            F.z = Scalar(0.0);
            energy = Scalar(0.0);
            for (unsigned int i = 0; i < 6; i++)
                virial[i] = Scalar(0.0);

			// convert type as little as possible
			vec3<Scalar> position = vec3<Scalar>(m_pos);
			vec3<Scalar> dxv;
			// initialize virial
			bool energy_shift = true;
			for (unsigned int k = 0; k < field.numSpheres; k++)
				{
				dxv = wall_eval_dist(field.m_Spheres[k], position, m_box);
				Scalar3 dx = -vec_to_scalar3(dxv);

				// calculate r_ij squared (FLOPS: 5)
	            Scalar rsq = dot(dx, dx);
				if (rsq > params.ronsq)
					{
		            // compute the force and potential energy
		            Scalar force_divr = Scalar(0.0);
		            Scalar pair_eng = Scalar(0.0);
					//cout << "evaluator params " << params.params.x << " " << params.params.y << "rcut = " << params.rcutsq << " ronsq = "<< params.ronsq << endl;
		            evaluator eval(rsq, params.rcutsq, params.params); //TODO: Fix hardcoding
		            bool evaluated = eval.evalForceAndEnergy(force_divr, pair_eng, energy_shift);

		            if (evaluated)
		                {

		                //Scalar force_div2r = force_divr; // removing half since the other "particle" won't be represented * Scalar(0.5);
		                // add the force, potential energy and virial to the particle i
		                // (FLOPS: 8)
		                F += dx*force_divr;
		                energy += pair_eng; // removing half since the other "particle" won't be represented * Scalar(0.5);
	                    virial[0] += force_divr*dx.x*dx.x;
	                    virial[1] += force_divr*dx.x*dx.y;
	                    virial[2] += force_divr*dx.x*dx.z;
	                    virial[3] += force_divr*dx.y*dx.y;
	                    virial[4] += force_divr*dx.y*dx.z;
	                    virial[5] += force_divr*dx.z*dx.z;
	                    cout << "Force is " << F.x << ", " << F.y << ", " << F.z <<endl;
		               	cout << "dx is " << dx.x << ", " << dx.y << ", " << dx.z <<endl;
						}
					}
				}

			for (unsigned int k = 0; k < field.numCylinders; k++)
				{
				dxv = wall_eval_dist(field.m_Cylinders[k], position, m_box);
				Scalar3 dx = vec_to_scalar3(dxv);

				// calculate r_ij squared (FLOPS: 5)
	            Scalar rsq = dot(dx, dx);

	            if (rsq > params.ronsq)
		            {
		            // compute the force and potential energy
		            Scalar force_divr = Scalar(0.0);
		            Scalar pair_eng = Scalar(0.0);
		            evaluator eval(rsq, params.rcutsq, params.params);
		            bool evaluated = eval.evalForceAndEnergy(force_divr, pair_eng, energy_shift);

		            if (evaluated)
		                {
		                F += dx*force_divr;
		                energy += pair_eng;
	                    virial[0] += force_divr*dx.x*dx.x;
	                    virial[1] += force_divr*dx.x*dx.y;
	                    virial[2] += force_divr*dx.x*dx.z;
	                    virial[3] += force_divr*dx.y*dx.y;
	                    virial[4] += force_divr*dx.y*dx.z;
	                    virial[5] += force_divr*dx.z*dx.z;
						}
					}
				}
			for (unsigned int k = 0; k < field.numPlanes; k++)
				{
				dxv = wall_eval_dist(field.m_Planes[k], position, m_box);
				Scalar3 dx = vec_to_scalar3(dxv);

				// calculate r_ij squared (FLOPS: 5)
	            Scalar rsq = dot(dx, dx);

	            if (rsq > params.ronsq)
		            {
		            // compute the force and potential energy
		            Scalar force_divr = Scalar(0.0);
		            Scalar pair_eng = Scalar(0.0);
		            evaluator eval(rsq, params.rcutsq, params.params);

		            bool evaluated = eval.evalForceAndEnergy(force_divr, pair_eng, energy_shift);

		            if (evaluated)
		                {
		                //Scalar force_div2r = force_divr; // removing half since the other "particle" won't be represented * Scalar(0.5);
		                // add the force, potential energy and virial to the particle i
		                // (FLOPS: 8)
		                F += dx*force_divr;
		                energy += pair_eng; // removing half since the other "particle" won't be represented * Scalar(0.5);
	                    virial[0] += force_divr*dx.x*dx.x;
	                    virial[1] += force_divr*dx.x*dx.y;
	                    virial[2] += force_divr*dx.x*dx.z;
	                    virial[3] += force_divr*dx.y*dx.y;
	                    virial[4] += force_divr*dx.y*dx.z;
	                    virial[5] += force_divr*dx.z*dx.z;
						}
					}
				}
			};

        #ifndef NVCC
        //! Get the name of this potential
        /*! \returns The potential name. Must be short and all lowercase, as this is the name energies will be logged as
            via analyze.log.
        */
        static std::string getName()
            {
            return std::string("walls_") + evaluator::getName();
            }
        #endif

    protected:
        Scalar3 		m_pos;                //!< particle position
        BoxDim 			m_box;                 //!< box dimensions
        unsigned int 	idx;
        field_type 		field;			  //!< contains all information about the walls.
        param_type 		params;
	};

template < class evaluator >
typename EvaluatorWalls<evaluator>::param_type make_wall_params(typename evaluator::param_type p, Scalar rcutsq, Scalar ronsq)
	{
	typename EvaluatorWalls<evaluator>::param_type params;
	params.params = p;
	params.rcutsq = rcutsq;
	params.ronsq = ronsq;
	return params;
	}

template< class evaluator>
void export_wall_param_helpers()
	{
	class_<typename EvaluatorWalls<evaluator>::param_type , boost::shared_ptr<typename EvaluatorWalls<evaluator>::param_type> >((EvaluatorWalls<evaluator>::getName()+"_params").c_str(), init<>())
		.def_readwrite("params", &EvaluatorWalls<evaluator>::param_type::params)
		.def_readwrite("ronsq", &EvaluatorWalls<evaluator>::param_type::ronsq)
		.def_readwrite("rcutsq", &EvaluatorWalls<evaluator>::param_type::rcutsq)
		;
	def(std::string("make_"+EvaluatorWalls<evaluator>::getName()+"_params").c_str(), &make_wall_params<evaluator>);
	}

template <class evaluator>
void export_PotentialExternalWall(const std::string& name)
	{
	export_PotentialExternal< PotentialExternal<EvaluatorWalls<evaluator> > >(name);
	export_wall_param_helpers<evaluator>();
	}


#endif //__EVALUATOR__WALLS_H__
