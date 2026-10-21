import unittest
from unittest.mock import patch
from run_allocation_smoke import ScientistCampaignRoute
from route import Route, RouteError

class Runner(unittest.TestCase):
    def route(self):return ScientistCampaignRoute(lambda *a:None,lambda *a,**k:None,'xcom1')
    def test_checkpoint_runs_matrix_before_return(self):
        r=self.route();seen=[]
        def matrix(title,scientists):
            seen.append((title,scientists));r.state={'state':'research_info','visible':{'allocated':'0'}}
        r.staff_matrix=matrix
        r._run=lambda:r.confirm_project_title('Laser Weapons')
        with patch.object(Route,'confirm_project_title',return_value={}):r.run()
        self.assertEqual(seen,[('Laser Weapons',True)])
    def test_failed_matrix_not_swallowed(self):
        r=self.route()
        def failure(*a,**kw):raise RouteError('postcondition')
        r.staff_matrix=failure;r._run=lambda:r.confirm_project_title('Laser Weapons')
        with patch.object(Route,'confirm_project_title',return_value={}):
            with self.assertRaisesRegex(RouteError,'postcondition'):r.run()
    def test_title_failure_before_matrix(self):
        r=self.route();seen=[];r.staff_matrix=lambda *a,**kw:seen.append(1)
        r._run=lambda:r.confirm_project_title('wrong')
        with patch.object(Route,'confirm_project_title',side_effect=RouteError('title')):
            with self.assertRaises(RouteError):r.run()
        self.assertEqual(seen,[])
    def test_missing_checkpoint_fails(self):
        r=self.route();r._run=lambda:None
        with self.assertRaisesRegex(RouteError,'not_reached'):r.run()

if __name__=='__main__':unittest.main()
