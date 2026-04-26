cmake_minimum_required(VERSION 3.8)
project(so101_description)

# Find dependencies
find_package(ament_cmake REQUIRED)

# Install the folders so ROS can find them
# Add or update the install block before ament_package()
	install(DIRECTORY
	  launch
	  urdf
	  assets
	  DESTINATION share/${PROJECT_NAME}
	)
ament_package()
