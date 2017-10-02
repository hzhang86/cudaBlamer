/*
 *  Copyright 2014-2017 Hui Zhang
 *  Previous contribution by Nick Rutar 
 *  All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package blame;

public class Global {

	public static String testProgram;
	public static int typeOfTest;  //0, standard blame, 1, data centric reads 
	public static int totalInstances;
	public static int totalInstByNode[];
	public static int totalNodes;
	public static boolean useMetaData;
	public static String rootDir;
	
	//global var to indicate whether we are process exclusive blame
    public static boolean exclusive_blame;
    //interval = THRESHOLD/frequency
    public static final double SAMPLE_INTERVAL = 0.3835;
}
