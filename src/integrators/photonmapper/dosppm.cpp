/*
    This file is part of Mitsuba, a physically based rendering system.

    Copyright (c) 2007-2014 by Wenzel Jakob and others.

    Mitsuba is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License Version 3
    as published by the Free Software Foundation.

    Mitsuba is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <mitsuba/core/plugin.h>
#include <mitsuba/core/bitmap.h>
#include <mitsuba/render/gatherproc.h>
#include <mitsuba/render/renderqueue.h>

#include <iostream>
#include <fstream>
#include <string>

#include <utility>


#if defined(MTS_OPENMP)
# include <omp.h>
#endif

MTS_NAMESPACE_BEGIN

/*!\plugin{dosppm}{Distributed out of core stochastic progressive photon mapping integrator}
 * \order{8}
 * \parameters{
 *     \parameter{maxDepth}{\Integer}{Specifies the longest path depth
 *         in the generated output image (where \code{-1} corresponds to $\infty$).
 *	       A value of \code{1} will only render directly visible light sources.
 *	       \code{2} will lead to single-bounce (direct-only) illumination,
 *	       and so on. \default{\code{-1}}
 *	   }
 *     \parameter{photonCount}{\Integer}{Number of photons to be shot per iteration\default{250000}}
 *     \parameter{initialRadius}{\Float}{Initial radius of gather points in world space units.
 *         \default{0, i.e. decide automatically}}
 *     \parameter{alpha}{\Float}{Radius reduction parameter \code{alpha} from the paper\default{0.7}}
 *     \parameter{granularity}{\Integer}{
		Granularity of photon tracing work units for the purpose
		of parallelization (in \# of shot particles) \default{0, i.e. decide automatically}
 *     }
 *	   \parameter{rrDepth}{\Integer}{Specifies the minimum path depth, after
 *	      which the implementation will start to use the ``russian roulette''
 *	      path termination criterion. \default{\code{5}}
 *	   }
 *     \parameter{maxPasses}{\Integer}{Maximum number of passes to render (where \code{-1}
 *        corresponds to rendering until stopped manually). \default{\code{-1}}}
 * }
 * This plugin implements stochastic progressive photon mapping by Hachisuka et al.
 * \cite{Hachisuka2009Stochastic}. This algorithm is an extension of progressive photon
 * mapping (\pluginref{ppm}) that improves convergence
 * when rendering scenes involving depth-of-field, motion blur, and glossy reflections.
 *
 * Note that the implementation of \pluginref{sppm} in Mitsuba ignores the sampler
 * configuration---hence, the usual steps of choosing a sample generator and a desired
 * number of samples per pixel are not necessary. As with \pluginref{ppm}, once started,
 * the rendering process continues indefinitely until it is manually stopped.
 *
 * \remarks{
 *    \item Due to the data dependencies of this algorithm, the parallelization is
 *    limited to the local machine (i.e. cluster-wide renderings are not implemented)
 *    \item This integrator does not handle participating media
 *    \item This integrator does not currently work with subsurface scattering
 *    models.
 * }
 */
//TODO: Move these structs into a header
 class iAABB {
 public:
 	Point3i min;
 	Point3i max;
 	iAABB(Point3i min=Point3i(), Point3i max=Point3i()): min(min), max(max){}
  AABB toAABB(float grid_size){
    Point min = Point(this->min[0] * grid_size,
      this->min[1] * grid_size,
      this->min[2] * grid_size);
    Point max = Point(this->max[0] * grid_size,
        this->max[1] * grid_size,
        this->max[2] * grid_size);
    return AABB(min, max);
  }
 	friend std::ostream& operator << ( std::ostream& o, const iAABB& e ) {
 		 o << "min: " << e.min[0] << " " << e.min[1] << " " <<e.min[2]
		   <<", max: " << e.max[0] << " " << e.max[1] << " " <<e.max[2];
 		 return o;
 	}
 };
 struct Portal {
    Point3i min;
    Point3i max;
    int cut_axis;
    int chunk1;
    int chunk2;

    friend std::ostream& operator << (std::ostream& o, const Portal& p) {
 		 o << "Axis " << p.cut_axis << endl
           << "Coords " << p.min[0] << " " << p.min[1] << " " << p.min[2] << " ; "
           << p.max[0] << " " << p.max[1] << " " << p.max[2] << endl
           << "Chunks " << p.chunk1 << " | " << p.chunk2;

 		 return o;
 	}
 };
class DOSPPMIntegrator : public Integrator {
public:
	/// Represents one individual PPM gather point including relevant statistics
	struct GatherPoint {
		Intersection its;
		Float radius;
		Spectrum weight;
		Spectrum flux;
		Spectrum emission;
		Float N;
		int depth;
		Point2i pos;

		inline GatherPoint() : weight(0.0f), flux(0.0f), emission(0.0f), N(0.0f) { }
	};

	DOSPPMIntegrator(const Properties &props) : Integrator(props) {
		/* Initial photon query radius (0 = infer based on scene size and sensor resolution) */
		m_initialRadius = props.getFloat("initialRadius", 0);
		/* Alpha parameter from the paper (influences the speed, at which the photon radius is reduced) */
		m_alpha = props.getFloat("alpha", .7);
		/* Number of photons to shoot in each iteration */
		m_photonCount = props.getInteger("photonCount", 250000);
		/* Granularity of the work units used in parallelizing the
		   particle tracing task (default: choose automatically). */
		m_granularity = props.getInteger("granularity", 0);
		/* Longest visualized path length (<tt>-1</tt>=infinite). When a positive value is
		   specified, it must be greater or equal to <tt>2</tt>, which corresponds to single-bounce
		   (direct-only) illumination */
		m_maxDepth = props.getInteger("maxDepth", -1);
		/* Depth to start using russian roulette */
		m_rrDepth = props.getInteger("rrDepth", 3);
		/* Indicates if the gathering steps should be canceled if not enough photons are generated. */
		m_autoCancelGathering = props.getBoolean("autoCancelGathering", true);
		/* Maximum number of passes to render. -1 renders until the process is stopped. */
		m_maxPasses = props.getInteger("maxPasses", -1);
		m_mutex = new Mutex();
		if (m_maxDepth <= 1 && m_maxDepth != -1)
			Log(EError, "Maximum depth must be set to \"2\" or higher!");
		if (m_maxPasses <= 0 && m_maxPasses != -1)
			Log(EError, "Maximum number of Passes must either be set to \"-1\" or \"1\" or higher!");
	}

	DOSPPMIntegrator(Stream *stream, InstanceManager *manager)
	 : Integrator(stream, manager) { }

	void serialize(Stream *stream, InstanceManager *manager) const {
		Integrator::serialize(stream, manager);
		Log(EError, "Network rendering is not supported!");
	}

	void cancel() {
		m_running = false;
	}

	int sum_poly(std::vector<unsigned int> poly_count, Point3i min, Point3i max, Point3i n_cell){
		unsigned int n_poly = 0;
		for(float x=min[0]; x < max[0]; x++){
			for(float y = min[1] ; y < max[1]; y++){
				for(float z = min[2]; z < max[2]; z++){
					int cell_id = x + y * n_cell[1] + z * n_cell[0] *  n_cell[1];
					n_poly += poly_count[cell_id];
				}
			}
		}
		return n_poly;
	}

	std::pair<iAABB, iAABB> split_in_chunks(std::vector<unsigned int> poly_count, iAABB * parent, Point3i n_cell, int& split_axis){
		Point3i min(parent->min), max(parent->max);
		unsigned int parent_poly_count = sum_poly(poly_count, min, max, n_cell);
		int min_axis = -1, min_i = -1;
		unsigned int min_polydifference = parent_poly_count;
		for(int axis = 0; axis < 3; axis++){
			Point3i offset_vector(0,0,0);
			offset_vector[axis] = 1;
			unsigned int current_polycount = 0;
			for(int i = min[axis]; i < max[axis]; i++){
				unsigned int slice_poly_count = sum_poly(poly_count, min, Point3i(max - offset_vector * (max[axis] - i - 1)), n_cell);
				current_polycount += slice_poly_count;
				unsigned int current_polydifference = abs(current_polycount - (parent_poly_count - current_polycount));
				if(current_polydifference < min_polydifference){
					min_axis = axis;
					min_i = i;
					min_polydifference = current_polydifference;
					cout << "Split output : Axis " << min_axis << " Split pos : " << min_i << " Poly diff : " << min_polydifference << endl;
				}
			}
		}
		Point3i min_offset_vector(0,0,0);
		min_offset_vector[min_axis] = 1;
        split_axis = min_axis;
		return std::make_pair(iAABB(min, Point3i(max - min_offset_vector * (max[min_axis] - min_i - 1))),
		                 iAABB(Point3i(min + min_offset_vector * (min_i+1)), max));
	}

	void subdivide_scene(const Scene *scene){

		// COMPUTE SCENE BB
		//cout << scene->toString() << "\n" << endl;
		AABB sceneBox = scene->getAABB();
		float grid_size = 1.;
		Point cell_offset = Point(grid_size, grid_size, grid_size);
		cout << "\nScene corners" << endl;
		for(int i = 0; i < 8; i++)
		  cout << sceneBox.getCorner(i).toString() << endl;
		cout << endl;

		// COMPUTE SCENE GRID
		Point min = sceneBox.getCorner(0);
		Point max = sceneBox.getCorner(7);
		std::vector<AABB> cells;
		Point3i n_cell(0, 0, 0); // Block count in each direction
		for(float x=min[0]; x < max[0]; x+= grid_size){
			for(float y = min[1] ; y < max[1]; y+= grid_size){
				for(float z = min[2]; z < max[2]; z+= grid_size){
					Point current(x, y, z);
					cells.push_back(AABB(current, current + cell_offset));
					n_cell[2]++;
				}
				n_cell[1]++;
			}
			n_cell[0]++;
		}
		n_cell[2] /= n_cell[1];
		n_cell[1] /= n_cell[0];
		cout << "NX:" << n_cell[0] << " NY:"<< n_cell[1] << " NZ:"<< n_cell[2] << endl;
		cout << "Cells : " << cells.size() << endl;

		// COMPUTE POLYGON NUMBER PER CELL
		std::vector<unsigned int> poly_count(cells.size());
		std::vector<TriMesh*> meshes = scene->getMeshes();
		//Count polygons
		for(unsigned int cell_id=0; cell_id<cells.size(); cell_id++){
			for(unsigned int mesh_id = 0; mesh_id < meshes.size(); mesh_id++){
				//const BSDF *bsdf = meshes[mesh_id]->getBSDF();
				// bsdf->getID();
				if(TAABB<Point>(cells[cell_id]).overlaps(meshes[mesh_id]->getAABB())){
					//Count poly in box
					Triangle * triangles = meshes[mesh_id]->getTriangles();
					for(unsigned int tri_id = 0; tri_id < meshes[mesh_id]->getTriangleCount(); tri_id++){
						if(TAABB<Point>(cells[cell_id]).overlaps(triangles[tri_id].getAABB(meshes[mesh_id]->getVertexPositions())))
						  poly_count[cell_id]++;
					}
				}
			}
		}
		for(unsigned int cell_id=0; cell_id<cells.size(); cell_id++)
		  cout << "Cell " << cell_id << " has " << poly_count[cell_id] << " polygons" << endl;


		// BUILD CHUNKS
		int split_depth = 2;
		std::vector<iAABB> i_chunks;
		std::pair<iAABB, iAABB> cur_chunks;

		i_chunks.push_back(iAABB(Point3i(0,0,0), n_cell));
		cout << endl;
		for(int depth = 0; depth < split_depth; depth++){
			std::vector<iAABB> tmp_chunks;
			for(unsigned int i = 0; i < i_chunks.size(); i++){
                int split_axis;
				cur_chunks = split_in_chunks(poly_count, &i_chunks[i], n_cell, split_axis);
				tmp_chunks.push_back(cur_chunks.first);
				tmp_chunks.push_back(cur_chunks.second);
			}
			i_chunks = tmp_chunks;
		}
		cout << endl;

        // Add portals
        std::vector<Portal> portals;
        if (i_chunks.size() > 1) {
            for (unsigned int i = 0; i < i_chunks.size(); i++) {
                TAABB<Point3i> bb1 = TAABB<Point3i>(i_chunks.at(i).min, i_chunks.at(i).max);

                for (unsigned int j = i + 1; j < i_chunks.size(); j++) {
                    TAABB<Point3i> bb2 = TAABB<Point3i>(i_chunks.at(j).min, i_chunks.at(j).max);

                    if (bb1.overlaps(bb2)) {
                        TAABB<Point3i> overlap = bb1;
                        overlap.clip(bb2);
                        int cut_axis = overlap.getShortestAxis();
                        Portal new_portal;

                        new_portal.min = overlap.min;
                        new_portal.max = overlap.max;
                        new_portal.cut_axis = cut_axis;

                        // Determine which chunk comes first on the cut axis
                        if (i_chunks.at(i).min[cut_axis] <= i_chunks.at(j).min[cut_axis]) {
                            new_portal.chunk1 = i;
                            new_portal.chunk2 = j;
                        } else {
                            new_portal.chunk1 = j;
                            new_portal.chunk2 = i;
                        }

                        portals.push_back(new_portal);
                    }
                }
            }
        }

        // Convert chunks from iAABBs to AABBs
		std::vector<AABB> chunks;
		for(std::vector<iAABB>::iterator it = i_chunks.begin(); it != i_chunks.end(); it++){
			chunks.push_back(it->toAABB(grid_size));
		}

		cout << "CHUNKS:" << endl;
		for(unsigned int i = 0; i < chunks.size(); i++){
			cout << "Index : "<< i_chunks[i] << endl;
		}
		cout << endl;

		cout << "SUBSCENE CREATION" << endl;
		createSubSceneTemplate(scene);
		for(unsigned int i = 0; i < chunks.size(); i++){
			createSubScene(scene, meshes, chunks[i], i, portals);
		}
		cout << endl;
	}

	void createSubSceneTemplate(const Scene *scene){

		//https://stackoverflow.com/questions/10195343/copy-a-file-in-a-sane-safe-and-efficient-way
	    //https://stackoverflow.com/questions/12463750/c-searching-text-file-for-a-particular-string-and-returning-the-line-number-wh

		cout << "This: " << this->toString() << endl;

		// Create the subscene folder and recreate it
		std::string folderPath("/tmp/subscene/");
		fs::path dir(folderPath.c_str());
		fs::remove_all(folderPath);
		fs::create_directory(dir);

		// Copy what we want from the original file
		const char * src_file = scene->getSourceFile().string().c_str();
		std::ifstream src(src_file);
		std::ofstream sceneTemplate("/tmp/subscene/subSceneTemplate.xml");
	    std::string line;
	    std::string shape("<shape");
	    std::string endShape("</shape");
        std::string integrator("<integrator type=");
        std::string envmap("<emitter type=\"envmap\"");
        std::string emitter("<emitter");
        std::string endScene("</scene");

        // For now I store the lights in all the chunks. Couldn't fiugre out a better way.
		while(getline(src, line)) {
		    if (line.find(shape, 0) != std::string::npos) {
		    	std::string emitterShape(line);
		        bool isEmit = false;
		        while(getline(src, line)) {
		        	emitterShape += "\n";
		        	emitterShape += line;
		        	if (line.find(emitter, 0) != std::string::npos) {
	        	 		isEmit = true;
	        	 	}
	        	 	if (line.find(endShape, 0) != std::string::npos) {
	        	 		if(isEmit){
	        	 			sceneTemplate << emitterShape << endl;
						}
        	 			break;
	        	 	}
		        }
		    }else if (line.find(envmap, 0) != std::string::npos) {
		        sceneTemplate << line << endl;
		    }else if (line.find(integrator, 0) != std::string::npos) {
		        sceneTemplate << integrator << "\"sppm\" >" << endl;
		    }else if(line.find(endScene, 0) != std::string::npos){
		    	break;
		    }else{
		    	sceneTemplate << line << endl;
		    }
		}
		//sceneTemplate << "<scene>" << endl;
		src.close();
		sceneTemplate.close();

	}

	void createSubScene(const Scene *scene, const std::vector<TriMesh*> meshes, AABB chunk, int chunkNb, std::vector<Portal> portals){

		// Create directory for this subscene
		std::string folderPath("");
		std::ostringstream oss;
		oss << "/tmp/subscene/sub" << chunkNb << "/";
		folderPath += oss.str();
		oss.str("");
		fs::path dir(folderPath.c_str());
		fs::create_directory(dir);

		// Copy texture files from original into subscene folder
		std::string sceneFile = scene->getSourceFile().string();
		std::string sceneTextPath = sceneFile.substr(0,sceneFile.size()-9) + "textures/";
		//cout << "src: " << sceneTextPath << " dest: " << folderPath + "textures/" << endl;
		fs::path source(sceneTextPath.c_str());
		fs::path dest((folderPath + "textures/").c_str());
		copyDir(source,dest);

		// Copy template into new scene file
		std::ifstream sceneTemplate("/tmp/subscene/subSceneTemplate.xml");
		std::ostringstream scenePath;
		scenePath << folderPath << "subscene" << chunkNb << ".xml";
		std::ofstream subscene(scenePath.str().c_str());
		subscene << sceneTemplate.rdbuf();

        // Add portals to the subscene
        for (std::vector<Portal>::iterator it = portals.begin(); it != portals.end(); it++) {
            int destination = -1;

            if (it->chunk1 == chunkNb)
                destination = it->chunk2;
            else if (it->chunk2 == chunkNb)
                destination = it->chunk1;
            else
                continue;

            subscene 	<< "\t<shape type=\"rectangle\" >\n"
						<< "\t\t<transform name=\"toWorld\" >\n"
						<< "\t\t\t<matrix value=\"1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\" />\n"
						<< "\t\t</transform>\n"
						<< "\t\t<ref id=\"DOSPPM_PortalTo_" << destination << "\" />\n"
						<< "\t</shape>" << endl;
            cout << "Portal added to subscene: " << *it << endl;
        }

		// find trimesh that are in the chunk, make an obj of the geometry actually in it and add it to the subscene

		// Loop through the meshes
		std::string chunkName("chunk");
		oss << chunkNb;
		chunkName += oss.str();

		for(unsigned int mesh_id = 0; mesh_id < meshes.size(); mesh_id++){

			// Check if the mesh intersects the chunk

			// @Arthur: tester si le mesh intersecte le chunk => ce test seul est déjà éliminatoire
			if(TAABB<Point>(chunk).overlaps(meshes[mesh_id]->getAABB())){
				// If so, make an obj out of the triangles that intersect the chunk
				//cout << mesh_id << endl;
				Triangle * f = meshes[mesh_id]->getTriangles();
				Normal * vn = meshes[mesh_id]->getVertexNormals();
				bool hasVertexNormals = meshes[mesh_id]->hasVertexNormals();
				Point2 * vt = meshes[mesh_id]->getVertexTexcoords();
				bool hasVertexTexcoords = meshes[mesh_id]->hasVertexTexcoords();
			 	Point * v = meshes[mesh_id]->getVertexPositions();

				// Make a new .obj from that file
				std::string objName(chunkName);
				std::ostringstream oss;
				oss << "_obj" << mesh_id << ".obj";
				objName += oss.str();
				oss.str("");

				std::ofstream outfile ((folderPath + objName).c_str(), std::ofstream::trunc);
				std::string vstr("");
				std::string vnstr("");
				std::string vtstr("");
				std::string fstr("");

				for(unsigned int i=0; i<meshes[mesh_id]->getVertexCount(); i++){

					// Add vertices to the .obj
					std::ostringstream oss;
					oss << "v " << v[i][0] << " " << v[i][1] << " " << v[i][2] << "\n";
					vstr += oss.str();
					oss.str("");
					// Check for segfaults then add vertices normals to the .obj
					if(hasVertexNormals){
						oss << "vn " << vn[i][0] << " " << vn[i][1] << " " << vn[i][2] << "\n";
						vnstr += oss.str();
						oss.str("");
					}
					// Check for segfaults then add vertices tex coords to the .obj
					if(hasVertexTexcoords){
						oss << "vt " << vt[i][0] << " " << vt[i][1] << "\n";
						vtstr += oss.str();
					}
				}

				for(unsigned int tri_id=0; tri_id<meshes[mesh_id]->getTriangleCount(); tri_id++){
					// @Arthur: tester si le triangle intersecte le chunk
					if(TAABB<Point>(chunk).overlaps(f[tri_id].getAABB(meshes[mesh_id]->getVertexPositions()))){
						std::ostringstream oss;
						oss << "f " << f[tri_id].idx[0]+1 << "/" << f[tri_id].idx[0]+1 << "/" << f[tri_id].idx[0]+1 << " " << f[tri_id].idx[1]+1 << "/" << f[tri_id].idx[1]+1 << "/" << f[tri_id].idx[1]+1 << " " << f[tri_id].idx[2]+1 << "/" << f[tri_id].idx[2]+1 << "/" << f[tri_id].idx[2]+1 << "\n";
						fstr += oss.str();
					}
				}


				//cout << vnstr << endl;
				outfile << vstr << std::endl;
				outfile << vnstr << std::endl;
				outfile << vtstr << std::endl;
				outfile << fstr << std::endl;
				outfile.close();

				const BSDF *bsdf = meshes[mesh_id]->getBSDF();
				subscene 	<< "\t<shape type=\"obj\" >\n"
							<< "\t\t<string name=\"filename\" value=\"" << objName << "\" />\n"
							<< "\t\t<transform name=\"toWorld\" >\n"
							<< "\t\t\t<matrix value=\"1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1\"/>\n"
							<< "\t\t</transform>\n"
							<< "\t\t<ref id=\"" << bsdf->getID() << "\" />\n"
							<< "\t</shape>" << endl;
				cout << "Created " << objName << endl;
			}
		}


		// Let's handle the lights now

		// C'est grave la merde, aucune idée de comment gérer ça

		const ref_vector<Emitter> &emitters = scene->getEmitters();
		cout << "Nb emitters: " << emitters.size() << endl;

		for(unsigned int i =0; i<emitters.size(); i++){
			const Emitter * emit = emitters[i].get();
			cout << "Is it env: " << emit->isEnvironmentEmitter() << endl;
			if(!emit->isEnvironmentEmitter()){
				const Shape * shape = emit->getShape();

			}
		}


		cout << "Objs for chunk " << chunkNb << " were created\n" << endl;
		subscene << "</scene>" << endl;
		sceneTemplate.close();
		subscene.close();
	}

	//https://stackoverflow.com/questions/8593608/how-can-i-copy-a-directory-using-boost-filesystem
	bool copyDir(fs::path const & source,fs::path const & destination)
	{
	    try
	    {
	        // Check whether the function call is valid
	        if(!fs::exists(source) || !fs::is_directory(source)){
	            std::cerr << "Source directory " << source.string()
	                << " does not exist or is not a directory." << '\n'
	            ;
	            return false;
	        }
	        // Create the destination directory
	        if(!fs::create_directory(destination))
	        {
	            std::cerr << "Unable to create destination directory"
	                << destination.string() << '\n'
	            ;
	            return false;
	        }
	    }
	    catch(fs::filesystem_error const & e)
	    {
	        std::cerr << e.what() << '\n';
	        return false;
	    }
	    // Iterate through the source directory
	    for(
	        fs::directory_iterator file(source);
	        file != fs::directory_iterator(); ++file
	    )
	    {
	        try
	        {
	            fs::path current(file->path());
	            if(fs::is_directory(current))
	            {
	                // Found directory: Recursion
	                if(
	                    !copyDir(
	                        current,
	                        destination / current.filename()
	                    )
	                )
	                {
	                    return false;
	                }
	            }
	            else
	            {
	                // Found file: Copy
	                fs::copy_file(
	                    current,
	                    destination / current.filename()
	                );
	            }
	        }
	        catch(fs::filesystem_error const & e)
	        {
	            std:: cerr << e.what() << '\n';
	        }
	    }
	    return true;
	}

	bool preprocess(const Scene *scene, RenderQueue *queue, const RenderJob *job,
			int sceneResID, int sensorResID, int samplerResID) {
		Integrator::preprocess(scene, queue, job, sceneResID, sensorResID, samplerResID);

		subdivide_scene(scene);


		if (m_initialRadius == 0) {
			/* Guess an initial radius if not provided
			  (use scene width / horizontal or vertical pixel count) * 5 */
			Float rad = scene->getBSphere().radius;
			Vector2i filmSize = scene->getSensor()->getFilm()->getSize();

			m_initialRadius = std::min(rad / filmSize.x, rad / filmSize.y) * 5;
		}
		return true;
	}

	bool render(Scene *scene, RenderQueue *queue,
		const RenderJob *job, int sceneResID, int sensorResID, int unused) {
		ref<Scheduler> sched = Scheduler::getInstance();
		ref<Sensor> sensor = scene->getSensor();
		ref<Film> film = sensor->getFilm();
		size_t nCores = sched->getCoreCount();
		Log(EInfo, "Starting render job (%ix%i, " SIZE_T_FMT " %s, " SSE_STR ") ..",
			film->getCropSize().x, film->getCropSize().y,
			nCores, nCores == 1 ? "core" : "cores");

		Vector2i cropSize = film->getCropSize();
		Point2i cropOffset = film->getCropOffset();

		m_gatherBlocks.clear();
		m_running = true;
		m_totalEmitted = 0;
		m_totalPhotons = 0;

		ref<Sampler> sampler = static_cast<Sampler *> (PluginManager::getInstance()->
			createObject(MTS_CLASS(Sampler), Properties("independent")));

		int blockSize = scene->getBlockSize();

		/* Allocate memory */
		m_bitmap = new Bitmap(Bitmap::ESpectrum, Bitmap::EFloat, film->getSize());
		m_bitmap->clear();
		for (int yofs=0; yofs<cropSize.y; yofs += blockSize) {
			for (int xofs=0; xofs<cropSize.x; xofs += blockSize) {
				m_gatherBlocks.push_back(std::vector<GatherPoint>());
				m_offset.push_back(Point2i(cropOffset.x + xofs, cropOffset.y + yofs));
				std::vector<GatherPoint> &gatherPoints = m_gatherBlocks[m_gatherBlocks.size()-1];
				int nPixels = std::min(blockSize, cropSize.y-yofs)
							* std::min(blockSize, cropSize.x-xofs);
				gatherPoints.resize(nPixels);
				for (int i=0; i<nPixels; ++i)
					gatherPoints[i].radius = m_initialRadius;
			}
		}

		/* Create a sampler instance for every core */
		std::vector<SerializableObject *> samplers(sched->getCoreCount());
		for (size_t i=0; i<sched->getCoreCount(); ++i) {
			ref<Sampler> clonedSampler = sampler->clone();
			clonedSampler->incRef();
			samplers[i] = clonedSampler.get();
		}

		int samplerResID = sched->registerMultiResource(samplers);

#ifdef MTS_DEBUG_FP
		enableFPExceptions();
#endif

#if defined(MTS_OPENMP)
		Thread::initializeOpenMP(nCores);
#endif

		int it = 0;
		while (m_running && (m_maxPasses == -1 || it < m_maxPasses)) {
			distributedRTPass(scene, samplers);
			photonMapPass(++it, queue, job, film, sceneResID,
					sensorResID, samplerResID);
		}

#ifdef MTS_DEBUG_FP
		disableFPExceptions();
#endif

		for (size_t i=0; i<samplers.size(); ++i)
			samplers[i]->decRef();

		sched->unregisterResource(samplerResID);
		return true;
	}

	void distributedRTPass(Scene *scene, std::vector<SerializableObject *> &samplers) {
		ref<Sensor> sensor = scene->getSensor();
		bool needsApertureSample = sensor->needsApertureSample();
		bool needsTimeSample = sensor->needsTimeSample();
		ref<Film> film = sensor->getFilm();
		Vector2i cropSize = film->getCropSize();
		Point2i cropOffset = film->getCropOffset();
		int blockSize = scene->getBlockSize();

		/* Process the image in parallel using blocks for better memory locality */
		Log(EInfo, "Creating %i gather points", cropSize.x*cropSize.y);
		#if defined(MTS_OPENMP)
			#pragma omp parallel for schedule(dynamic)
		#endif
		for (int i=0; i<(int) m_gatherBlocks.size(); ++i) {
			std::vector<GatherPoint> &gatherPoints = m_gatherBlocks[i];
			#if defined(MTS_OPENMP)
				Sampler *sampler = static_cast<Sampler *>(samplers[mts_omp_get_thread_num()]);
			#else
				Sampler *sampler = static_cast<Sampler *>(samplers[0]);
			#endif

			int xofs = m_offset[i].x, yofs = m_offset[i].y;
			int index = 0;
			for (int yofsInt = 0; yofsInt < blockSize; ++yofsInt) {
				if (yofsInt + yofs - cropOffset.y >= cropSize.y)
					continue;
				for (int xofsInt = 0; xofsInt < blockSize; ++xofsInt) {
					if (xofsInt + xofs - cropOffset.x >= cropSize.x)
						continue;
					Point2 apertureSample, sample;
					Float timeSample = 0.0f;
					GatherPoint &gatherPoint = gatherPoints[index++];
					gatherPoint.pos = Point2i(xofs + xofsInt, yofs + yofsInt);
					sampler->generate(gatherPoint.pos);
					if (needsApertureSample)
						apertureSample = sampler->next2D();
					if (needsTimeSample)
						timeSample = sampler->next1D();
					sample = sampler->next2D();
					sample += Vector2((Float) gatherPoint.pos.x, (Float) gatherPoint.pos.y);
					RayDifferential ray;
					sensor->sampleRayDifferential(ray, sample, apertureSample, timeSample);
					Spectrum weight(1.0f);
					int depth = 1;
					gatherPoint.emission = Spectrum(0.0f);

					while (true) {
						if (scene->rayIntersect(ray, gatherPoint.its)) {

							// TODO: CHECK IF PORTAIL?

							if (gatherPoint.its.isEmitter())
								gatherPoint.emission += weight * gatherPoint.its.Le(-ray.d);

							if (depth >= m_maxDepth && m_maxDepth != -1) {
								gatherPoint.depth = -1;
								break;
							}

							const BSDF *bsdf = gatherPoint.its.getBSDF();

							/* Create hit point if this is a diffuse material or a glossy
							   one, and there has been a previous interaction with
							   a glossy material */
							if ((bsdf->getType() & BSDF::EAll) == BSDF::EDiffuseReflection ||
								(bsdf->getType() & BSDF::EAll) == BSDF::EDiffuseTransmission ||
								(depth + 1 > m_maxDepth && m_maxDepth != -1)) {
								gatherPoint.weight = weight;
								gatherPoint.depth = depth;
								break;
							} else {
								/* Recurse for dielectric materials and (specific to SPPM):
								   recursive "final gathering" for glossy materials */
								BSDFSamplingRecord bRec(gatherPoint.its, sampler);
								weight *= bsdf->sample(bRec, sampler->next2D());
								if (weight.isZero()) {
									gatherPoint.depth = -1;
									break;
								}
								ray = RayDifferential(gatherPoint.its.p,
									gatherPoint.its.toWorld(bRec.wo), ray.time);
								++depth;
							}
						} else {
							/* Generate an invalid sample */
							gatherPoint.depth = -1;
							gatherPoint.emission += weight * scene->evalEnvironment(ray);
							break;
						}
					}
					sampler->advance();
				}
			}
		}
	}

	void photonMapPass(int it, RenderQueue *queue, const RenderJob *job,
			Film *film, int sceneResID, int sensorResID, int samplerResID) {
		Log(EInfo, "Performing a photon mapping pass %i (" SIZE_T_FMT " photons so far)",
				it, m_totalPhotons);
		ref<Scheduler> sched = Scheduler::getInstance();

		/* Generate the global photon map */
		ref<GatherPhotonProcess> proc = new GatherPhotonProcess(
			GatherPhotonProcess::EAllSurfacePhotons, m_photonCount,
			m_granularity, m_maxDepth == -1 ? -1 : m_maxDepth-1, m_rrDepth, true,
			m_autoCancelGathering, job);

		proc->bindResource("scene", sceneResID);
		proc->bindResource("sensor", sensorResID);
		proc->bindResource("sampler", samplerResID);

		sched->schedule(proc);
		sched->wait(proc);

		ref<PhotonMap> photonMap = proc->getPhotonMap();
		photonMap->build();
		Log(EDebug, "Photon map full. Shot " SIZE_T_FMT " particles, excess photons due to parallelism: "
			SIZE_T_FMT, proc->getShotParticles(), proc->getExcessPhotons());

		Log(EInfo, "Gathering ..");
		m_totalEmitted += proc->getShotParticles();
		m_totalPhotons += photonMap->size();
		film->clear();
		#if defined(MTS_OPENMP)
			#pragma omp parallel for schedule(dynamic)
		#endif
		for (int blockIdx = 0; blockIdx<(int) m_gatherBlocks.size(); ++blockIdx) {
			std::vector<GatherPoint> &gatherPoints = m_gatherBlocks[blockIdx];

			Spectrum *target = (Spectrum *) m_bitmap->getUInt8Data();
			for (size_t i=0; i<gatherPoints.size(); ++i) {
				GatherPoint &gp = gatherPoints[i];
				Float M, N = gp.N;
				Spectrum flux, contrib;

				if (gp.depth != -1) {
					M = (Float) photonMap->estimateRadianceRaw(
						gp.its, gp.radius, flux, m_maxDepth == -1 ? INT_MAX : m_maxDepth-gp.depth);
				} else {
					M = 0;
					flux = Spectrum(0.0f);
				}

				if (N == 0 && !gp.emission.isZero())
					gp.N = N = 1;

				if (N+M == 0) {
					gp.flux = contrib = Spectrum(0.0f);
				} else {
					Float ratio = (N + m_alpha * M) / (N + M);
					gp.radius = gp.radius * std::sqrt(ratio);

					gp.flux = (gp.flux +
							gp.weight * flux +
							gp.emission * (Float) proc->getShotParticles() * M_PI * gp.radius*gp.radius) * ratio;
					gp.N = N + m_alpha * M;
					contrib = gp.flux / ((Float) m_totalEmitted * gp.radius*gp.radius * M_PI);
				}

				target[gp.pos.y * m_bitmap->getWidth() + gp.pos.x] = contrib;
			}
		}
		film->setBitmap(m_bitmap);
		queue->signalRefresh(job);
	}

	std::string toString() const {
		std::ostringstream oss;
		oss << "DOSPPMIntegrator[" << endl
			<< "  maxDepth = " << m_maxDepth << "," << endl
			<< "  rrDepth = " << m_rrDepth << "," << endl
			<< "  initialRadius = " << m_initialRadius << "," << endl
			<< "  alpha = " << m_alpha << "," << endl
			<< "  photonCount = " << m_photonCount << "," << endl
			<< "  granularity = " << m_granularity << "," << endl
			<< "  maxPasses = " << m_maxPasses << endl
			<< "]";
		return oss.str();
	}

	MTS_DECLARE_CLASS()
private:
	std::vector<std::vector<GatherPoint> > m_gatherBlocks;
	std::vector<Point2i> m_offset;
	ref<Mutex> m_mutex;
	ref<Bitmap> m_bitmap;
	Float m_initialRadius, m_alpha;
	int m_photonCount, m_granularity;
	int m_maxDepth, m_rrDepth;
	size_t m_totalEmitted, m_totalPhotons;
	bool m_running;
	bool m_autoCancelGathering;
	int m_maxPasses;
};

MTS_IMPLEMENT_CLASS(DOSPPMIntegrator, false, Integrator)
MTS_EXPORT_PLUGIN(DOSPPMIntegrator, "Distributed out of core stochastic progressive photon mapper");
MTS_NAMESPACE_END
