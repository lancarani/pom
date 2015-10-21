//////////////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify         //
// it under the terms of the version 3 of the GNU General Public License        //
// as published by the Free Software Foundation.                                //
//                                                                              //
// This program is distributed in the hope that it will be useful, but          //
// WITHOUT ANY WARRANTY; without even the implied warranty of                   //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU             //
// General Public License for more details.                                     //
//                                                                              //
// You should have received a copy of the GNU General Public License            //
// along with this program. If not, see <http://www.gnu.org/licenses/>.         //
//                                                                              //
// Written by Francois Fleuret                                                  //
// (C) Ecole Polytechnique Federale de Lausanne                                 //
// Contact <pom@epfl.ch> for comments & bug reports                             //
//////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>

using namespace std;

#include "misc.h"
#include "global.h"
#include "vector.h"
#include "room.h"
#include "pom_solver.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/background_segm.hpp>
#include <stdio.h>
#include <iostream>

using namespace std;

void check_parameter(char *s, int line_number, char *buffer) {
  if(!s) {
    cerr << "Missing parameter line " << line_number << ":" << endl;
    cerr << buffer << endl;
    exit(1);
  }
}

bool is_number(char s[]) {
	for (unsigned i=0;i<sizeof(s);i++)
		if (!isdigit(s[i]))
			return false;
	return true;
}

int main(int argc, char **argv) {

  int numCamera = 0;
  int maxNumCamera = 10;
  string ipCamera[maxNumCamera];
  cv::VideoCapture camera[maxNumCamera];
  double cameraWidth[maxNumCamera];
  double cameraHeight[maxNumCamera];
  double cameraFps[maxNumCamera];

  bool update_bg_model = true;
  cv::BackgroundSubtractorMOG2 bgModel[maxNumCamera];
  cv::Mat img[maxNumCamera], fgmask[maxNumCamera], fgimg[maxNumCamera];

  ifstream *configuration_file = 0;
  istream *input_stream;

  cout << "POM OpenCV" << endl;

  if(argc > 2 || argc == 1) {
    cerr << argv[0] << " [-h | --help | <configuration file>]" << endl;
    exit(1);
  }
  else if(argc > 1) {
    if(strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      cout << argv[0] << " [-h | --help | <configuration file>]" << endl
           << endl
           << "  If a configuration file name is provided, the programs processes it" << endl
           << "  and prints information about the files it generates. Otherwise, it" << endl
           << "  reads the standard input and does not produce any output unless an" << endl
           << "  error occurs." << endl
           << endl;
      exit(0);
    }
    configuration_file = new ifstream(argv[1]);
    if(configuration_file->fail()) {
      cerr << "Can not open " << argv[1] << " for reading." << endl;
      exit(1);
    }
    input_stream = configuration_file;
  } else input_stream = &cin;

  char input_view_format[buffer_size] = "";
  char result_format[buffer_size] = "";
  char result_view_format[buffer_size] = "";
  char convergence_view_format[buffer_size] = "";

  char buffer[buffer_size], token[buffer_size];

  int line_number = 0;
  Vector<ProbaView *> *proba_views = 0;

  Room *room = 0;
  int grid_width = 0;
  int grid_height = 0;

  while(!input_stream->eof()) {

    input_stream->getline(buffer, buffer_size);
    line_number++;

    char *s = buffer;
    s = next_word(token, s, buffer_size);

    if(strcmp(token, "CAMERA") == 0) {
      char camera_ip[buffer_size] = "";
  	  check_parameter(s, line_number, buffer);
  	  s = next_word(camera_ip, s, buffer_size);
  	  ipCamera[numCamera] =  camera_ip;
  	  cout << "camera " << numCamera << " " << camera_ip << endl;
  	  numCamera++;
  	  for(int t=0;t<numCamera;t++) cout << "------ " << ipCamera[t] << endl;
  	}

    else if(strcmp(token, "ROOM") == 0) {
      int view_width = -1, view_height = -1;
      int nb_positions = -1;
      int nb_cameras = -1;

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      view_width = atoi(token);

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      view_height = atoi(token);

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      nb_cameras = atoi(token);

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      nb_positions = atoi(token);

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      grid_width = atoi(token);

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      grid_height = atoi(token);

      if(room) {
        cerr << "Room already defined, line" << line_number << "." << endl;
        exit(1);
      }

      room = new Room(view_width, view_height, nb_cameras, nb_positions);
      proba_views = new Vector<ProbaView *>(nb_cameras);
      for(int c = 0; c < proba_views->length(); c++)
        (*proba_views)[c] = new ProbaView(view_width, view_height);
    }

    else if(strcmp(token, "CONVERGENCE_VIEW_FORMAT") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(convergence_view_format, s, buffer_size);
    }

    else if(strcmp(token, "INPUT_VIEW_FORMAT") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(input_view_format, s, buffer_size);
    }

    else if(strcmp(token, "RESULT_VIEW_FORMAT") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(result_view_format, s, buffer_size);
    }

    else if(strcmp(token, "RESULT_FORMAT") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(result_format, s, buffer_size);
    }

    else if(strcmp(token, "PROCESS") == 0) {
		/** open camera */
		for (int i=0;i<numCamera;i++) {
			if (isdigit(ipCamera[i][0])) {
				cout << "deviceid " << ipCamera[i] << endl;
				camera[i].open(atoi(ipCamera[i].c_str()));
			}
			else {
				cout << "http " << ipCamera[i] << endl;
				/** http, local avi path */
				camera[i].open(ipCamera[i]);
			}

			if (!camera[i].isOpened()) {
				cout << "Cannot open the video cam " << i << endl;
				return -1;
			}
			cameraWidth[i] = camera[i].get(CV_CAP_PROP_FRAME_WIDTH);
			cameraHeight[i] = camera[i].get(CV_CAP_PROP_FRAME_HEIGHT);
			cameraFps[i] = camera[i].get(CV_CAP_PROP_FPS);
			cout << "camera : " << i << " Frame size : " << cameraWidth[i] << " x " << cameraHeight[i] << " fps " << cameraFps[i] << endl;
		}

		RGBImage tmp(room->_view_width, room->_view_height);

		POMSolver solver(room);

		Vector<scalar_t> prior(room->nb_positions());
		Vector<scalar_t> proba_presence(room->nb_positions());
		for(int i = 0; i < room->nb_positions(); i++) prior[i] = global_prior;

		if(strcmp(input_view_format, "") == 0) {
			cerr << "You must specify the input view format." << endl;
			exit(1);
		}

		int f = 1;
		int countImg = 0;
		while (true) {
			/** get frame for each camera */
			cv::Mat frame[maxNumCamera];
			for(int i=0;i<numCamera;i++) {
				/** read a new frame from video */
				bool bSuccess = camera[i].read(frame[i]);
				if (!bSuccess) {
					 cout << "Cannot read a frame from video stream camera : " << i << endl;
					 break;
				}
			}
			/** background subtractor and populate pom model */
			for (int i=0;i<numCamera;i++) {
				camera[i] >> img[i];
				if (img[i].rows == 0) return -1;
				/** resize frame input room->_view_width, room->_view_height  */
				cv::resize(img[i], img[i], cv::Size(room->_view_width, room->_view_height));
				/** imshow resized input frame */
				stringstream ss;
				ss << "c:" << i;
				cv::imshow(ss.str(), img[i]);
				if( img[i].empty() ) break;
				/** background subtractor */
				if( fgimg[i].empty() )
				  fgimg[i].create(img[i].size(), img[i].type());

				//update the background model
				bgModel[i](img[i], fgmask[i], update_bg_model ? -1 : 0);
				fgimg[i] = cv::Scalar::all(0);
				img[i].copyTo(fgimg[i], fgmask[i]);
				cv::Mat bgimg;
				bgModel[i].getBackgroundImage(bgimg);
				stringstream ssfm, ssfi;
				ssfm << "fm:" << i;
				imshow(ssfm.str(), fgmask[i]);
				//ssfi << "foreground image : " << i;
				//imshow(ssfi.str(), fgimg[i]);

				/** input frame delle n camere all'algoritmo pom */
				pomsprintf(buffer, buffer_size, input_view_format, i, f, 0);
				tmp.read_opencv_mat(fgmask[i]);
				(*proba_views)[i]->from_image(&tmp);
			}

			/* pom sol */
			if(strcmp(convergence_view_format, "") != 0)
			  solver.solve(room, &prior, proba_views, &proba_presence, f, convergence_view_format);
			else
			  solver.solve(room, &prior, proba_views, &proba_presence, f, 0);

			/** output pom */
			if(strcmp(result_view_format, "") != 0)
			  for(int c = 0; c < room->nb_cameras(); c++) {
				pomsprintf(buffer, buffer_size, result_view_format, c, f, 0);
				//if(configuration_file)
				//cout << "Saving " << buffer << endl;
				room->save_stochastic_view(buffer, c, (*proba_views)[c], &proba_presence);
			  }

			/** output proba_presence */
			int pos = 0;
			int grid_step = 10;
			cv::Mat grid2d((grid_height * grid_step) + 1, (grid_width * grid_step) + 1, CV_8UC3, cv::Scalar(0,0,0));
			for (int i=0;i<grid_width;i++) {
				for(int j=0;j<grid_height;j++) {
					cv::rectangle(grid2d,
							cv::Point( i*grid_step, j*grid_step),
							cv::Point( i*grid_step + grid_step, j*grid_step + grid_step),
							cv::Scalar(255*(1-proba_presence[pos]), 255*(1-proba_presence[pos]), 255*(1-proba_presence[pos])), -1);
					cv::rectangle(grid2d,
							cv::Point( i*grid_step, j*grid_step),
							cv::Point( i*grid_step + grid_step, j*grid_step + grid_step),
							cv::Scalar(0,0,0));
					pos++;

				}
			}
			cv::imshow("map2d", grid2d);

			/** keyboard input */
			char k = (char)cv::waitKey(30);
			/** background subtractor switch on-off background model update */
			if( k == ' ' ) {
				update_bg_model = !update_bg_model;
				if(update_bg_model)
					printf("Background update is on\n");
				else
					printf("Background update is off\n");
			}
			/** save frame */
			if (k == 's') {
				for(int i=0;i<numCamera;i++) {
					stringstream ss;
					ss << "cam" << i << "_" << countImg << ".jpg";
					imwrite(ss.str(), frame[i]);
				}
				countImg++;
			}
			/** exit */
			if (k == 27) {
				cout << "esc key is pressed by user" << endl;
				break;
		   }
		}
    }

    else if(strcmp(token, "RECTANGLE") == 0) {
      int n_camera, n_position;

      if(!room) {
        cerr << "You must define a room before adding rectangles, line" << line_number << "." << endl;
        exit(1);
      }

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      n_camera = atoi(token);

      if(n_camera < 0 || n_camera >= room->nb_cameras()) {
        cerr << "Out of range camera number line " << line_number << "." << endl;
        exit(1);
      }

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      n_position = atoi(token);

      if(n_position < 0 || n_camera >= room->nb_positions()) {
        cerr << "Out of range position number line " << line_number << "." << endl;
        exit(1);
      }

      Rectangle *current = room->avatar(n_camera, n_position);

      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      if(strcmp(token, "notvisible") == 0) {
        current->visible = false;
        current->xmin = -1;
        current->ymin = -1;
        current->xmax = -1;
        current->ymax = -1;
      } else {
        current->visible = true;
        current->xmin = atoi(token);
        check_parameter(s, line_number, buffer);
        s = next_word(token, s, buffer_size);
        current->ymin = atoi(token);
        check_parameter(s, line_number, buffer);
        s = next_word(token, s, buffer_size);
        current->xmax = atoi(token);
        check_parameter(s, line_number, buffer);
        s = next_word(token, s, buffer_size);
        current->ymax = atoi(token);

        if(current->xmin < 0 || current->xmax >= room->view_width() ||
           current->ymin < 0 || current->ymax >= room->view_height()) {
          cerr << "Rectangle out of bounds, line " << line_number << endl;
          exit(1);
        }
      }
    }

    else if(strcmp(token, "PRIOR") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      global_prior = atof(token);
    }

    else if(strcmp(token, "SIGMA_IMAGE_DENSITY") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      global_sigma_image_density = atof(token);
    }

    else if(strcmp(token, "SMOOTHING_COEFFICIENT") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      global_smoothing_coefficient = atof(token);
    }

    else if(strcmp(token, "MAX_NB_SOLVER_ITERATIONS") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      global_max_nb_solver_iterations = atoi(token);
    }

    else if(strcmp(token, "ERROR_MAX") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      global_error_max = atof(token);
    }

    else if(strcmp(token, "NB_STABLE_ERROR_FOR_CONVERGENCE") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      global_nb_stable_error_for_convergence = atoi(token);
    }

    else if(strcmp(token, "PROBA_IGNORED") == 0) {
      check_parameter(s, line_number, buffer);
      s = next_word(token, s, buffer_size);
      global_proba_ignored = atof(token);
      cout << "global_proba_ignored = " << global_proba_ignored << endl;
    }

    else if(strcmp(buffer, "") == 0 || buffer[0] == '#') { }

    else {
      cerr << "Unknown token " << token << ".";
      exit(1);
    }
  }

  if(proba_views)
    for(int c = 0; c < proba_views->length(); c++) delete (*proba_views)[c];

  delete proba_views;
  delete room;

  delete configuration_file;
}
