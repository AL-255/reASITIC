{{ fullname | escape | underline }}

.. automodule:: {{ fullname }}
   :members:
   :show-inheritance:
   :inherited-members:
   :undoc-members:

{% block modules %}
{% if modules %}
.. rubric:: Modules

.. autosummary::
   :toctree:
   :template: module.rst
   :recursive:

{% for item in modules %}
   {{ item }}
{%- endfor %}
{% endif %}
{% endblock %}
