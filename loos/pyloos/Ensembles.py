import numpy
import numpy.linalg
import loos



def averageStructure(traj):
    """
    Returns the average structure for a trajectory.  The subset defined
    in the trajectory controls what is averaged over.  The trajectory
    may actually be a VirtualTrajectory or an AlignedVirtualTrajectory.
    """
    avg = numpy.zeros((len(traj.currentFrame()), 3))
    for frame in traj:
        coords = frame.getCoords()
        avg += coords
    avg /= len(traj)

    structure = traj.currentFrame().copy()
    for i in range(len(structure)):
        structure[i].coords(loos.GCoord(avg[i][0], avg[i][1], avg[i][2]))
        
    return(structure)


def extractCoords(traj):
    """
    Returns the coordinates for an entire trajectory as an MxN numpy matrix,
    where M is 3*natoms and N is the length of the trajectory.  The subset
    in the trajectory controls what is extracted.  The trajectory
    may actually be a VirtualTrajectory or an AlignedVirtualTrajectory.
    """
    m = len(traj.currentFrame()) * 3
    n = len(traj)

    A = numpy.zeros((m, n))
    for i in range(n):
        coords = traj[i].getCoords()
        A[:,i] = numpy.reshape(coords, (1,m))
    return(A)



def svd(traj):
    """
    Returns a tuple containing the SVD result for a trajectory, along with
    the average structure.  The tuple is,
      (left-singular-vectors, singular-values, right-singular-vectors, average)

    The subset set in the trajectory controls what is used for the SVD.  The
    trajectory may be a VirtualTrajectory or an AlignedVirtualTrajectory.
    """
    A = extractCoords(traj)
    avg = numpy.average(A, 1)
    avg = avg[:,numpy.newaxis]
    A -= avg
    (U,s,V) = numpy.linalg.svd(A)

    structure = traj.currentFrame().copy()
    for i in range(len(structure)):
        structure[i].coords(loos.GCoord(avg[i*3], avg[i*3+1], avg[i*3+2))

    return(U,s,V,structure)
