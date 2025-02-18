#include <boost/filesystem.hpp>

#include <stencila/component.hpp>

namespace Stencila {

Component& Component::initialise(const std::string& address){
	std::string path = Component::locate(address);
	if(path.length()) Component::path(path);
	else STENCILA_THROW(Exception,"No component found with address <"+address+">");      
	return *this;
}

std::string Component::path(bool ensure) const {
	if(meta_){
		if(meta_->path.length()==0 and ensure){
			// Remove constness so that the setter can be called
			const_cast<Component&>(*this).path(std::string(""));
		}
		return meta_->path;
	} else {
		if(ensure){
			const_cast<Component&>(*this).path(std::string(""));
			return meta_->path;
		}
		else return "";
	}
}

Component& Component::path(const std::string& path) {
	using namespace boost::filesystem;
	if(not meta_) meta_ = new Meta;
	std::string current_path = meta_->path;
	std::string new_path = path;
	
	// Create and canonicalise the new path (to follow symlinks etc)
	// so comparing apples with apples below
	if(new_path.length()>0) {
		// Create the path if necessary (canonical won't work otherwise)
		if(not exists(new_path)) create_directories(new_path);
		new_path = canonical(new_path).string();
	}

	// If the current path is empty...
	if(current_path.length()==0){
		// If the new path is empty then...
		if(new_path.length()==0){
			// Create a unique one
			boost::filesystem::path unique = Host::temp_dirname();
			create_directories(unique);
			meta_->path = unique.string();
		} else {
			meta_->path = new_path;
		}
	} 
	// If the current path is not empty...
	else {
		// and the new path is not empty...
		if(new_path.length()>0){
			// and they are different...
			if(new_path != current_path){
				// move (i.e rename) existing path to the new path.
				rename(current_path,new_path);
				meta_->path = new_path;
			}
		}
	}
	return *this;
}

Component& Component::path(const char* path){
	return Component::path(std::string(path));
}

std::string Component::address(void) const {
	using namespace boost::filesystem;
	std::string path = this->path();
	// Is path in any of the stores?
	if(path.length()>0){
		for(auto store : Host::stores()){
			if(path.length()>store.length()){
				if(path.substr(0,store.length())==store){
					// Component is in a store
					return path.substr(store.length()+1);
				}
			}
		}
	}
	// Component is not in a store so return a "local" address 
	// i.e. a canonicalised path
	return boost::filesystem::canonical(path).generic_string();
}

std::string Component::address(bool ensure){
	if(not ensure) STENCILA_THROW(Exception,"Method must be called with a true value");
	path(true);
	return address();
}

Component& Component::address(const std::string& address) {
	path(Host::stores()[0] + "/" + address);
	return *this;
}


Component& Component::address(const char* address) {
	return this->address(std::string(address));
}

std::string Component::locate(const std::string& address){
	using namespace boost::filesystem;
	if(address.length()>0){
		// Check if this is an address explicty declared as a local path...
		bool local = false;
		if(address[0]=='/' or address[0]=='.') local = true;
		else if(address.length()>=2){
			if(address[1]==':') local = true;
		}
		if(local){
			// Check the local path actually exists on the filesystem
			if(exists(address)){
				return canonical(address).generic_string();
			}
			else STENCILA_THROW(Exception,"Local address (leading '/', '.', or 'x:') does not correspond to a local filesystem path:\n  address: "+address);
		}
		else {
			// Could be a local path or an address within a store
			if(exists(address)){
				// Local path so just return that
				return canonical(address).generic_string();
			} else {
				// Not a local path so search in stores
				for(std::string store : Host::stores()){
					auto path = boost::filesystem::path(store)/address;
					if(exists(path)) return path.generic_string();
				}
			}
		}
	}
	return "";
}

std::vector<Component::File> Component::list(const std::string& subdirectory){
	using namespace boost::filesystem;
	std::vector<File> files;
	std::string dir = boost::filesystem::path(path()).parent_path().string() + subdirectory;
	if(exists(dir) and is_directory(dir)){
		directory_iterator end ;
		for(directory_iterator iter(dir) ; iter != end ; ++iter){
			File file;
			file.name = iter->path().filename().string();
			if(is_regular_file(iter->status())) file.type = "f";
			else if(is_directory(iter->status())) file.type = "d";
			else file.type = "o";
			files.push_back(file);
		}
	}
	// Sort alphabetically
	std::sort(files.begin(),files.end(),File::by_name);
	return files;
}

Component& Component::destroy(void){
	boost::filesystem::path path_full = Component::path();
	if(boost::filesystem::exists(path_full)){
		boost::filesystem::remove_all(path_full);
	}
	return *this;
}

Component& Component::create(const std::string& path,const std::string& content){
	boost::filesystem::path path_full(Component::path(true));
	path_full /= path;
	if(not boost::filesystem::exists(path_full)){
		std::ofstream file(path_full.string());
		file<<content;
		file.close();
	}
	return *this;
}

Component& Component::write_to(const std::string& path, const std::string& content){
	boost::filesystem::path path_full(Component::path(true));
	path_full /= path;
	std::ofstream file(path_full.string());
	file<<content;
	file.close();
	return *this;
}

std::string Component::read_from(const std::string& path) const {
	boost::filesystem::path path_full(Component::path());
	path_full /= path;
	std::ifstream file(path_full.string());
	std::stringstream stream;
	stream<<file.rdbuf();
	return stream.str();
}

Component& Component::delete_(const std::string& path){
	boost::filesystem::path path_full = Component::path();
	path_full /= path;
	if(boost::filesystem::exists(path_full)){
		boost::filesystem::remove_all(path_full);
	}
	return *this;
}

Component& Component::read(const std::string& path){
	std::string path_copy = path;
	if(path_copy.length()==0){
		path_copy = this->path();
		if(path_copy.length()==0) STENCILA_THROW(Exception,"Component path not supplied and not yet set.");
	}
	else {
		if(not boost::filesystem::exists(path_copy)) STENCILA_THROW(Exception,"Directory does not exist.\n  path: "+path_copy);
		if(not boost::filesystem::is_directory(path_copy)) STENCILA_THROW(Exception,"Path is not a directory.\n  path: "+path_copy);
		this->path(path_copy);
	}
	return *this;
}

Component& Component::write(const std::string& path){
	this->path(path);
	return *this;
}

Component& Component::vacuum(void) {
	auto dir = path();
	if (dir.length()) {
		auto out = dir + "/out/";
		if (boost::filesystem::exists(out)) {
			boost::filesystem::remove_all(out);
		}
	}
	return *this;
}

}
