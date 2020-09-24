"""
%%fullcopyright(2018)
"""

import os
from . import parsers
from .project import Project
from .exceptions import InvalidProjectElement, ParseError
from aliases import Aliases



#xml elements and attributes
project_xpath = "./project"
dependencies_xpath = "./dependencies/project"
name_attr_str = "name"
path_attr_str = "path"
default_attr_str = "default"
xml_attr_str = "xml"


class ParseWorkspace(object):
    """Parse an x2w workspace"""
    def __init__(self, workspace_file):
        self.workspace_xml = parsers.get_xml(workspace_file)
        self.aliases = Aliases(workspace_file)
        self.default_project = None

    def __repr__(self):
        return self.workspace_xml

    def __call__(self):
        return self.workspace_xml

    def _join_workspace_path_with_relative_project_path(self, relative_project_path):
        return os.path.abspath(os.path.join(self.workspace_xml.base_dir, relative_project_path))

    def _get_absolute_project_path(self, project_xml):
        project_path = self._parse_project_path(project_xml)
        if not os.path.isabs(project_path):
            return self._join_workspace_path_with_relative_project_path(project_path)
        return project_path

    def _parse_project_path(self, element):
        if element is not None:
            return self.aliases.expand(element.get(path_attr_str))
        else:
            raise InvalidProjectElement("Project element has no name or path attribute")

    def _parse_project_attributes(self, element):
        if element is not None:
            name = element.get(name_attr_str)
            path = element.get(path_attr_str)
            return name, path
        else:
            raise InvalidProjectElement("Project element has no name or path attribute")

    def _get_project_id_from_path(self, element):
        project_path = self._parse_project_path(element)
        return os.path.splitext(os.path.basename(project_path))[0]

    def _parse_project_name(self, element):
        project_id = element.get(name_attr_str)
        if project_id is None:
            project_id = self._get_project_id_from_path(element)
        return project_id

    def _parse_dependent_projects(self, parsed_project_xpath):
        dependencies = []
        for dependency in parsed_project_xpath.findall(dependencies_xpath):
            dependencies.append(self._parse_project_name(dependency))
        return dependencies

    def _update_project_dependencies(self, parsed_project_xpath, projects):
        for project_xml in parsed_project_xpath:
            project_id = self._parse_project_name(project_xml)
            dependencies = self._parse_dependent_projects(project_xml)
            for dependency in dependencies:
                projects[project_id].dependencies.append(projects[dependency])

    def _parse_project(self, project_name, project_xml):
        project_file = self._get_absolute_project_path(project_xml)
        project = Project(project_name, project_file, sdk=self.aliases.sdk, wsroot=self.aliases.wsroot)
        return project

    @staticmethod
    def _project_is_default(element):
        return element.get(default_attr_str) == "yes"

    def _parse_projects_from_xml_source(self, parsed_project_xpath):
        projects = {}
        for project_xml in parsed_project_xpath:
            project_id = self._parse_project_name(project_xml)
            project = self._parse_project(project_id, project_xml)
            if self._project_is_default(project_xml):
                if self.default_project:
                    raise InvalidProjectElement((
                        "More than one default project in: {ws}.\n"
                        "Found: {new}\n"
                        "Already had: {old}\n"
                    ).format(ws=self.workspace_xml.filename, old=self.default_project.name, new=project.name))
                self.default_project = project
            projects[project_id] = project
        if not self.default_project:
            raise ParseError("Default project not found in workspace: {}".format(self.workspace_xml.filename))
        return projects

    def parse(self):
        # This is a 2 stage process: 1. Parse all projects. 2. determine dependent projects

        parsed_project_xpath = self.workspace_xml.parse().findall(project_xpath)
        projects = self._parse_projects_from_xml_source(parsed_project_xpath)
        self._update_project_dependencies(parsed_project_xpath, projects)
        return projects
