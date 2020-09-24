"""
%%fullcopyright(2018)
"""

from .parse_workspace import ParseWorkspace
from workspace_builders.base_builder import DefaultBuilder
from workspace_builders.base_builder import BuildError


class Workspace(dict):
    'Representation of a Workspace. Iterates over projects'
    def __init__(self, xml_source, builder=DefaultBuilder()):
        self.xml_source = xml_source
        self.builder = builder

    def parse(self):
        parser = ParseWorkspace(self.xml_source)
        self.update(parser.parse())
        self.default_project = parser.default_project
        return self

    def build(self):
        """ Build all projects in the workspace

            Returns:
                int -- Error_code or 0 if successful
        """
        return self.parse().__build(self.values())

    def build_default_project(self):
        """ Build only the default project and its dependencies

            Returns:
                int -- Error_code or 0 if successful
        """
        return self.parse().__build([self.default_project])

    def __build(self, project_list=None):
        """ Build a list of projects and its dependencies

        Keyword Arguments:
            project_list {list} -- list of projects to build (default: {None})
        
        Returns:
            int -- Error_code or 0 if successful
        """
        try:
            for project in project_list:
                self.builder.build(project)
            self.builder.final_build()
        except BuildError as e:
            print(e)
            return 1
        return 0