#include "pstreams/pstream.h"
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>

//Super simple and stupid class for interactive
//plotting and visualisation from C++ using gnuplot
namespace cplot
{
	//A plot contains multiple axes
	// A plot has a single location so axes are overlaid
	//
	//Axes contain multiple lines


	class Plotter
	{
		//std::vector<plot> plots;

		std::vector<std::string> plots, pdata, extra;
		redi::opstream plot;

		public:

			std::string range;

			Plotter()
			:plot("gnuplot")
			{
				plot << "set term x11 noraise\n";
				newline("");
			}


			std::ostream& s()
			{
				return plot;
			}

			void add_extra(const std::string& s)
			{
				extra.push_back(s);
			}

			Plotter& newline(const std::string& ps)
			{
				plots.push_back(ps);
				pdata.push_back("");
				return *this;
			}
			
			template<class C> Plotter& addpt(const C& pt)
			{
				using std::isfinite;
				std::ostringstream o;

				if(isfinite(pt))
					o << pt << std::endl;
				else
					skip();

				pdata.back() += o.str();
				return *this;
			}
			
			template<class C> Plotter& addpt(C p1, C p2)
			{
				using std::isfinite;
				std::ostringstream o;

				if(isfinite(p1) && isfinite(p2))
					o << p1 << " " << p2 << std::endl;
				else
					skip();
		
				pdata.back() += o.str();
				return *this;
			}

			template<class D> Plotter& addpts(const D& pt)
			{
				using std::isfinite;
				std::ostringstream o;
				for(unsigned int i=0; i < pt.size(); i++)
					if(isfinite(pt[i]))
						o << pt[i] << std::endl;
					else
						skip();

				pdata.back() += o.str();
				return *this;
			}
		
			Plotter& skip()
			{
				pdata.back() += "\n";
				return *this;
			}

			void draw()
			{
				using std::cerr;
				using std::endl;

				bool data=0;

				//Check for data
				std::vector<int> have_data(plots.size());
				for(unsigned int i=0; i < plots.size(); i++)
					for(unsigned int j=0; j < pdata[i].size(); j++)
						if(!std::isspace(pdata[i][j]))
						{
							have_data[i] = 1;
							data=1;
							break;
						}

				for(unsigned int i=0, first=1; i < plots.size(); i++)
					if(have_data[i])
					{
						if(first)
						{
							plot << "plot " << range << " \"-\"";
							first=0;
						}
						else
						{
							plot << ", \"-\"";
						}

						if(plots[i] != "")
						{
							plot << " with " << plots[i];
						}
					}
				
				if(data)
					plot << std::endl;
				for(unsigned int i=0; i < plots.size(); i++)
					if(have_data[i])
					{
						plot << pdata[i] << "e\n";
					}

				if(data)
					plot << std::flush;

				for(unsigned int i=0; i < extra.size(); i++)
						plot << extra[i] << "\n";

				
				plots.clear();
				extra.clear();
				pdata.clear();
				newline("");
			}
	};
}
