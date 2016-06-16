#include "Scene_polyhedron_shortest_path_item.h"

#include "Scene_polylines_item.h"

#include <vector>
#include <Qt>
#include <QKeySequence>
#include <fstream>

#include <CGAL/Surface_mesh_shortest_path/function_objects.h>
#include <QString>
struct Scene_polyhedron_shortest_path_item_priv
{
  typedef CGAL::Three::Scene_interface::Bbox Bbox;

  typedef boost::property_map<Polyhedron, CGAL::vertex_point_t>::type VertexPointMap;

  typedef boost::graph_traits<Polyhedron> GraphTraits;
  typedef GraphTraits::face_descriptor face_descriptor;
  typedef GraphTraits::face_iterator face_iterator;

  typedef CGAL::Surface_mesh_shortest_path_traits<Kernel, Polyhedron> Surface_mesh_shortest_path_traits;
  typedef CGAL::Surface_mesh_shortest_path<Surface_mesh_shortest_path_traits> Surface_mesh_shortest_path;
  typedef Surface_mesh_shortest_path::Face_location Face_location;
  typedef CGAL::AABB_face_graph_triangle_primitive<Polyhedron, VertexPointMap> AABB_face_graph_primitive;
  typedef CGAL::AABB_traits<Kernel, AABB_face_graph_primitive> AABB_face_graph_traits;
  typedef CGAL::AABB_tree<AABB_face_graph_traits> AABB_face_graph_tree;

  typedef Surface_mesh_shortest_path_traits::Barycentric_coordinate Barycentric_coordinate;
  typedef Surface_mesh_shortest_path_traits::Construct_barycentric_coordinate Construct_barycentric_coordinate;
  typedef Surface_mesh_shortest_path_traits::Ray_3 Ray_3;
  typedef Surface_mesh_shortest_path_traits::Point_3 Point_3;
  typedef Surface_mesh_shortest_path_traits::FT FT;
  Scene_polyhedron_shortest_path_item_priv(Scene_polyhedron_shortest_path_item *parent)
    : m_shortestPaths(NULL)
    , m_isTreeCached(false)
    , m_shiftHeld(false)
  {
    item = parent;
  }

  bool get_mouse_ray(QMouseEvent* mouseEvent, Kernel::Ray_3&);
  void recreate_shortest_path_object();
  void ensure_aabb_object();
  void ensure_shortest_paths_tree();

  bool run_point_select(const Kernel::Ray_3&);
  void remove_nearest_point(const Scene_polyhedron_shortest_path_item::Face_location& ray);
  void get_as_edge_point(Scene_polyhedron_shortest_path_item::Face_location& inOutLocation);
  void get_as_vertex_point(Scene_polyhedron_shortest_path_item::Face_location& inOutLocation);
  void initialize_buffers(CGAL::Three::Viewer_interface *viewer = 0) const;
  void compute_elements(void) const;
  void deinitialize()
  {
    if (m_shortestPaths)
    {
      delete m_shortestPaths;
      m_sceneInterface = NULL;
    }
  }

  enum Selection_mode
  {
    INSERT_POINTS_MODE = 0,
    REMOVE_POINTS_MODE = 1,
    SHORTEST_PATH_MODE = 2
  };

  enum Primitives_mode
  {
    VERTEX_MODE = 0,
    EDGE_MODE = 1,
    FACE_MODE = 2
  };

  enum VAOs {
      Selected_Edges=0,
      NbOfVaos
  };
  enum VBOs {
      Vertices = 0,
      NbOfVbos
  };

  Scene_polyhedron_shortest_path_item* item;
  Messages_interface* m_messages;
  QMainWindow* m_mainWindow;
  CGAL::Three::Scene_interface* m_sceneInterface;
  Scene_polyhedron_shortest_path_item::Surface_mesh_shortest_path* m_shortestPaths;
  Scene_polyhedron_shortest_path_item::AABB_face_graph_tree m_aabbTree;
  std::string m_deferredLoadFilename;
  Scene_polyhedron_shortest_path_item::Selection_mode m_selectionMode;
  Scene_polyhedron_shortest_path_item::Primitives_mode m_primitivesMode;
  bool m_isTreeCached;
  bool m_shiftHeld;

  mutable std::vector<float> vertices;
  mutable QOpenGLShaderProgram *program;
  mutable bool are_buffers_filled;
};
Scene_polyhedron_shortest_path_item::Scene_polyhedron_shortest_path_item()
   :Scene_polyhedron_item_decorator(NULL, false)
{
  d = new Scene_polyhedron_shortest_path_item_priv(this);
}

Scene_polyhedron_shortest_path_item::Scene_polyhedron_shortest_path_item(Scene_polyhedron_item* polyhedronItem, CGAL::Three::Scene_interface* sceneInterface, Messages_interface* messages, QMainWindow* mainWindow)
  :Scene_polyhedron_item_decorator(polyhedronItem, false)
{ d = new Scene_polyhedron_shortest_path_item_priv(this);
  initialize(polyhedronItem, sceneInterface, messages, mainWindow);
}
  
Scene_polyhedron_shortest_path_item::~Scene_polyhedron_shortest_path_item()
{
  deinitialize();
  delete d;
}

void Scene_polyhedron_shortest_path_item_priv::compute_elements() const
{

    vertices.resize(0);

    for(Scene_polyhedron_shortest_path_item::Surface_mesh_shortest_path::Source_point_iterator it = m_shortestPaths->source_points_begin(); it != m_shortestPaths->source_points_end(); ++it)
    {
      const Kernel::Point_3& p = m_shortestPaths->point(it->first, it->second);
      vertices.push_back(p.x());
      vertices.push_back(p.y());
      vertices.push_back(p.z());
    }

}

void Scene_polyhedron_shortest_path_item_priv::initialize_buffers(CGAL::Three::Viewer_interface* viewer)const
{
    //vao containing the data for the selected lines
    {
        program = item->getShaderProgram(Scene_polyhedron_shortest_path_item::PROGRAM_NO_SELECTION, viewer);
        item->vaos[Selected_Edges]->bind();
        program->bind();
        item->buffers[Vertices].bind();
        item->buffers[Vertices].allocate(vertices.data(), vertices.size()*sizeof(float));
        program->enableAttributeArray("vertex");
        program->setAttributeBuffer("vertex",GL_FLOAT,0,3);
        item->buffers[Vertices].release();
        item->vaos[Selected_Edges]->release();
    }
    are_buffers_filled = true;
}
bool Scene_polyhedron_shortest_path_item::supportsRenderingMode(RenderingMode m) const
{
  switch (m)
  {
  case Points:
    return true;
  case PointsPlusNormals:
    return true;
  case Wireframe:
    return true;
  case Flat:
    return true;
  case FlatPlusEdges:
    return true;
  case Gouraud:
    return true;
  default:
    return true;
  }
}
  
void Scene_polyhedron_shortest_path_item::draw(CGAL::Three::Viewer_interface* viewer) const
{
    if (supportsRenderingMode(renderingMode()))
    {
      drawPoints(viewer);
    }
}


void Scene_polyhedron_shortest_path_item::drawPoints(CGAL::Three::Viewer_interface* viewer) const
{
    if(!are_buffers_filled)
    {
        d->initialize_buffers(viewer);
    }
   glPointSize(4.0f);
   d->program = getShaderProgram(PROGRAM_NO_SELECTION);
   attribBuffers(viewer, PROGRAM_NO_SELECTION);
   vaos[Scene_polyhedron_shortest_path_item_priv::Selected_Edges]->bind();
   d->program->bind();
   d->program->setAttributeValue("colors", QColor(Qt::green));
   viewer->glDrawArrays(GL_POINTS, 0, d->vertices.size()/3);
   d->program->release();
   vaos[Scene_polyhedron_shortest_path_item_priv::Selected_Edges]->release();
   glPointSize(1.0f);
}
  
Scene_polyhedron_shortest_path_item* Scene_polyhedron_shortest_path_item::clone() const
{
  return 0;
}

void Scene_polyhedron_shortest_path_item::set_selection_mode(Selection_mode mode)
{
  d->m_selectionMode = mode;
}

Scene_polyhedron_shortest_path_item::Selection_mode Scene_polyhedron_shortest_path_item::get_selection_mode() const
{
  return d->m_selectionMode;
}

void Scene_polyhedron_shortest_path_item::set_primitives_mode(Primitives_mode mode)
{
  d->m_primitivesMode = mode;
}

Scene_polyhedron_shortest_path_item::Primitives_mode Scene_polyhedron_shortest_path_item::get_primitives_mode() const
{
  return d->m_primitivesMode;
}

void Scene_polyhedron_shortest_path_item_priv::recreate_shortest_path_object()
{
  if (m_shortestPaths)
  {
    delete m_shortestPaths;
  }

  m_shortestPaths = new Scene_polyhedron_shortest_path_item::Surface_mesh_shortest_path(*(item->polyhedron()),
            CGAL::get(boost::vertex_index, *(item->polyhedron())),
            CGAL::get(CGAL::halfedge_index, *(item->polyhedron())),
            CGAL::get(CGAL::face_index, *(item->polyhedron())),
            CGAL::get(CGAL::vertex_point, *(item->polyhedron())));
            
  //m_shortestPaths->m_debugOutput = true;

  m_isTreeCached = false;
}

void Scene_polyhedron_shortest_path_item_priv::ensure_aabb_object()
{
  if (!m_isTreeCached)
  {
    m_shortestPaths->build_aabb_tree(m_aabbTree);
    m_isTreeCached = true;
  }
}

void Scene_polyhedron_shortest_path_item_priv::ensure_shortest_paths_tree()
{
  if (!m_shortestPaths->changed_since_last_build())
  {
    m_messages->information("Recomputing shortest paths tree...");
    m_shortestPaths->build_sequence_tree();
    m_messages->information("Done.");
  }
}
  
void Scene_polyhedron_shortest_path_item::poly_item_changed()
{
  d->recreate_shortest_path_object();
  invalidateOpenGLBuffers();
  Q_EMIT itemChanged();
}
  
void Scene_polyhedron_shortest_path_item::invalidateOpenGLBuffers()
{
  d->compute_elements();
  compute_bbox();
  are_buffers_filled = false;

}

bool Scene_polyhedron_shortest_path_item_priv::get_mouse_ray(QMouseEvent* mouseEvent, Kernel::Ray_3& outRay)
{
  bool found = false;
  
  QGLViewer* viewer = *QGLViewer::QGLViewerPool().begin();
  qglviewer::Camera* camera = viewer->camera();
  const qglviewer::Vec point = camera->pointUnderPixel(mouseEvent->pos(), found);
  
  if(found)
  {
    const qglviewer::Vec orig = camera->position();
    outRay = Ray_3(Point_3(orig.x, orig.y, orig.z), Point_3(point.x, point.y, point.z));
  }
  
  return found;
}

void Scene_polyhedron_shortest_path_item_priv::remove_nearest_point(const Scene_polyhedron_shortest_path_item::Face_location& faceLocation)
{
  Surface_mesh_shortest_path_traits::Compute_squared_distance_3 computeSquaredDistance3;
  
  const Point_3 pickLocation = m_shortestPaths->point(faceLocation.first, faceLocation.second);
  
  Surface_mesh_shortest_path::Source_point_iterator found = m_shortestPaths->source_points_end();
  FT minDistance(0.0);
  const FT thresholdDistance = FT(0.4);
  
  for (Surface_mesh_shortest_path::Source_point_iterator it = m_shortestPaths->source_points_begin(); it != m_shortestPaths->source_points_end(); ++it)
  {
    Point_3 sourceLocation = m_shortestPaths->point(it->first, it->second);
    FT distance = computeSquaredDistance3(sourceLocation, pickLocation);
    
    if ((found == m_shortestPaths->source_points_end() && distance <= thresholdDistance) || distance < minDistance)
    {
      found = it;
      minDistance = distance;
    }
  }
  
  if (found != m_shortestPaths->source_points_end())
  {
    m_shortestPaths->remove_source_point(found);
  }
}

void Scene_polyhedron_shortest_path_item_priv::get_as_edge_point(Scene_polyhedron_shortest_path_item::Face_location& inOutLocation)
{
  size_t minIndex = 0;
  FT minCoord(inOutLocation.second[0]);
  
  for (size_t i = 1; i < 3; ++i)
  {
    if (minCoord > inOutLocation.second[i])
    {
      minIndex = i;
      minCoord = inOutLocation.second[i];
    }
  }
  
  // The nearest edge is that of the two non-minimal barycentric coordinates
  size_t nearestEdge[2];
  size_t current = 0;
  
  for (size_t i = 0; i < 3; ++i)
  {
    if (i != minIndex)
    {
      nearestEdge[current] = i;
      ++current;
    }
  }

  Construct_barycentric_coordinate construct_barycentric_coordinate;

  Point_3 trianglePoints[3] = {
    m_shortestPaths->point(inOutLocation.first, construct_barycentric_coordinate(FT(1.0), FT(0.0), FT(0.0))),
    m_shortestPaths->point(inOutLocation.first, construct_barycentric_coordinate(FT(0.0), FT(1.0), FT(0.0))),
    m_shortestPaths->point(inOutLocation.first, construct_barycentric_coordinate(FT(0.0), FT(0.0), FT(1.0))),
  };
  
  CGAL::Surface_mesh_shortest_paths_3::Parametric_distance_along_segment_3<Surface_mesh_shortest_path_traits> parametricDistanceSegment3;
  
  Point_3 trianglePoint = m_shortestPaths->point(inOutLocation.first, inOutLocation.second);
  
  FT distanceAlongSegment = parametricDistanceSegment3(trianglePoints[nearestEdge[0]], trianglePoints[nearestEdge[1]], trianglePoint);
  
  FT coords[3] = { FT(0.0), FT(0.0), FT(0.0), };
  
  coords[nearestEdge[1]] = distanceAlongSegment;
  coords[nearestEdge[0]] = FT(1.0) - distanceAlongSegment;

  inOutLocation.second = construct_barycentric_coordinate(coords[0], coords[1], coords[2]);
}

void Scene_polyhedron_shortest_path_item_priv::get_as_vertex_point(Scene_polyhedron_shortest_path_item::Face_location& inOutLocation)
{
  size_t maxIndex = 0;
  FT maxCoord(inOutLocation.second[0]);
  
  for (size_t i = 1; i < 3; ++i)
  {
    if (inOutLocation.second[i] > maxCoord)
    {
      maxIndex = i;
      maxCoord = inOutLocation.second[i];
    }
  }
  
  FT coords[3] = { FT(0.0), FT(0.0), FT(0.0), };
  coords[maxIndex] = FT(1.0);
  
  Construct_barycentric_coordinate construct_barycentric_coordinate;
  inOutLocation.second = construct_barycentric_coordinate(coords[0], coords[1], coords[2]);
}

bool Scene_polyhedron_shortest_path_item_priv::run_point_select(const Ray_3& ray)
{
  ensure_aabb_object();
  
  Face_location faceLocation = m_shortestPaths->locate(ray, m_aabbTree);
  
  if (faceLocation.first == GraphTraits::null_face())
  {
    m_messages->information(QObject::tr("Shortest Paths: No face under cursor."));
    return false;
  }
  else
  {
    m_messages->information(QObject::tr("Shortest Paths: Selected Face: %1; Barycentric coordinates: %2 %3 %4")
      .arg(faceLocation.first->id())
      .arg(double(faceLocation.second[0]))
      .arg(double(faceLocation.second[1]))
      .arg(double(faceLocation.second[2])));
    switch (m_selectionMode)
    {
    case INSERT_POINTS_MODE:
      switch (m_primitivesMode)
      {
      case VERTEX_MODE:
        get_as_vertex_point(faceLocation);
        m_shortestPaths->add_source_point(faceLocation.first, faceLocation.second);
        break;
      case EDGE_MODE:
        get_as_edge_point(faceLocation);
        m_shortestPaths->add_source_point(faceLocation.first, faceLocation.second);
        break;
      case FACE_MODE:
        m_shortestPaths->add_source_point(faceLocation.first, faceLocation.second);
        break;
      }
      break;
    case REMOVE_POINTS_MODE:
      remove_nearest_point(faceLocation);
      break;
    case SHORTEST_PATH_MODE:
      switch (m_primitivesMode)
      {
      case VERTEX_MODE:
        get_as_vertex_point(faceLocation);
        break;
      case EDGE_MODE:
        get_as_edge_point(faceLocation);
        break;
      case FACE_MODE:
        break;
      }
      
      if (m_shortestPaths->number_of_source_points() > 0)
      {
        ensure_shortest_paths_tree();
        
        Scene_polylines_item* polylines = new Scene_polylines_item();
            
        polylines->polylines.push_back(Scene_polylines_item::Polyline());
            
        m_messages->information(QObject::tr("Computing shortest path polyline..."));

        QTime time;
        time.start();
        //~ m_shortestPaths->m_debugOutput=true;
        m_shortestPaths->shortest_path_points_to_source_points(faceLocation.first, faceLocation.second, std::back_inserter(polylines->polylines.back()));
        std::cout << "ok (" << time.elapsed() << " ms)" << std::endl;
        
        polylines->setName(QObject::tr("%1 (shortest path)").arg(item->polyhedron_item()->name()));
        polylines->setColor(Qt::red);
        this->m_sceneInterface->setSelectedItem(-1);
        this->m_sceneInterface->addItem(polylines);
        this->m_sceneInterface->changeGroup(polylines, item->parentGroup());
      }
      else
      {
        m_messages->warning(QObject::tr("No source points to compute shortest paths from."));
      }
      break;
    }
    item->invalidateOpenGLBuffers();
    return true;
  }
}



bool Scene_polyhedron_shortest_path_item::eventFilter(QObject* /*target*/, QEvent* event)
{
  if(event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
  {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
    d->m_shiftHeld = modifiers.testFlag(Qt::ShiftModifier);
  }
  
  if (event->type() == QEvent::MouseButtonPress && d->m_shiftHeld)
  {
    QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
    if(mouseEvent->button() == Qt::LeftButton) 
    {
      Ray_3 mouseRay;
      
      if (d->get_mouse_ray(mouseEvent, mouseRay))
      {
        if (d->run_point_select(mouseRay))
        {
          return true;
        }
      }
    }
  }
  
  return false;
}

bool Scene_polyhedron_shortest_path_item::load(const std::string& file_name)
{
  d->m_deferredLoadFilename = file_name;
  return true;
}

bool Scene_polyhedron_shortest_path_item::deferred_load(Scene_polyhedron_item* polyhedronItem, CGAL::Three::Scene_interface* sceneInterface, Messages_interface* messages, QMainWindow* mainWindow)
{
  initialize(polyhedronItem, sceneInterface, messages, mainWindow);
  
  std::ifstream inFile(d->m_deferredLoadFilename.c_str());
  
  if (!inFile) 
  { 
    return false;
  }
  
  d->m_shortestPaths->clear();
  
  std::vector<face_descriptor> listOfFaces;
  listOfFaces.reserve(CGAL::num_faces(*polyhedron()));
  face_iterator current, end;
  for (boost::tie(current, end) = CGAL::faces(*polyhedron()); current != end; ++current)
  {
    listOfFaces.push_back(*current);
  }

  std::string line;
  std::size_t faceId;
  Barycentric_coordinate location;
  Construct_barycentric_coordinate construct_barycentric_coordinate;

  while (std::getline(inFile, line))
  {
    std::istringstream lineStream(line);
    FT coords[3];
    lineStream >> faceId >> coords[0] >> coords[1] >> coords[2];
    
    location = construct_barycentric_coordinate(coords[0], coords[1], coords[2]);
    
    // std::cout << "Read in face: " << faceId << " , " << location << std::endl;
    
    d->m_shortestPaths->add_source_point(listOfFaces[faceId], location);
  }

  return true;
}

bool Scene_polyhedron_shortest_path_item::save(const std::string& file_name) const 
{
  std::ofstream out(file_name.c_str());
  
  if (!out)
  { 
    return false; 
  }

  for(Surface_mesh_shortest_path::Source_point_iterator it = d->m_shortestPaths->source_points_begin(); it != d->m_shortestPaths->source_points_end(); ++it)
  { 
    // std::cout << "Output face location: " << it->first->id() << " , " << it->second << std::endl;
    out << it->first->id() << " " << it->second[0] << " " << it->second[1] << " " << it->second[3] << std::endl;
  }

  return true;
}

void Scene_polyhedron_shortest_path_item::initialize(Scene_polyhedron_item* polyhedronItem, CGAL::Three::Scene_interface* sceneInterface, Messages_interface* messages, QMainWindow* mainWindow)
{
  d->m_mainWindow = mainWindow;
  d->m_messages = messages;
  this->poly_item = polyhedronItem;
  d->m_sceneInterface = sceneInterface;
  connect(polyhedronItem, SIGNAL(item_is_about_to_be_changed()), this, SLOT(poly_item_changed()));
  QGLViewer* viewer = *QGLViewer::QGLViewerPool().begin();
  viewer->installEventFilter(this);
  d->m_mainWindow->installEventFilter(this);
  d->recreate_shortest_path_object();
}

void Scene_polyhedron_shortest_path_item::deinitialize()
{
  d->deinitialize();
  this->poly_item = NULL;
}

bool Scene_polyhedron_shortest_path_item::isFinite() const
{
  return true;
}

bool Scene_polyhedron_shortest_path_item::isEmpty() const 
{
  return false;
}

void Scene_polyhedron_shortest_path_item::compute_bbox() const
{
  _bbox = polyhedron_item()->bbox();
}

QString Scene_polyhedron_shortest_path_item::toolTip() const
{
  return QString();
}
