import unittest
from unittest.mock import patch
from production_route import ProductionRoute, ProductionCampaignRoute
from route import RouteError

class Checks(unittest.TestCase):
    def route(self):return ProductionRoute(lambda *a:None,lambda *a,**k:None,'xcom1')
    def test_legacy_scope_required(self):
        with self.assertRaisesRegex(RouteError,'requires_completion'):ProductionCampaignRoute(None,None,'xcom1')
    def test_history_not_overclaimed(self):
        with self.assertRaisesRegex(RouteError,'history_scope'):ProductionCampaignRoute(None,None,'xcom1',completion_allocation=True,verify_retained_history=True)
    def test_infinite_exact_visible_roundtrip(self):
        r=self.route();steps=[]
        state=lambda units:{'visible':dict(title='Laser Pistol',available='10',space='30',allocated='0',units=units)}
        r.ready=lambda _:state('1')
        def quantity(title,control,operation,value=None):
            steps.append((control,operation));return state('>\x01∞' if operation=='max' else '1')
        r.quantity=quantity;r.infinite_roundtrip('Laser Pistol')
        self.assertEqual(steps,[('units','max'),('units','max'),('units','clear')])
    def test_unknown_infinite_stops(self):
        r=self.route();base=dict(title='Laser Pistol',available='10',space='30',allocated='0',units='1')
        r.ready=lambda _:{'visible':base};calls=[]
        def q(*args):calls.append(args);return {'visible':dict(base,units=None)}
        r.quantity=q
        with self.assertRaisesRegex(RouteError,'not_visible'):r.infinite_roundtrip('Laser Pistol')
        self.assertEqual(len(calls),1)
    def test_wrong_detail_prevents_start(self):
        r=self.route();sent=[]
        def act(before,action,args=None,after=None,guard=None):
            if guard:guard({'visible':{'title':'Other'}})
            sent.append(action)
        r.act=act
        with self.assertRaisesRegex(RouteError,'title_unknown'):r.open_draft('Laser Pistol')
        self.assertNotIn('start_production',sent)
    def test_visible_completion_not_resource_shortage(self):
        for message,passes in [('Production of\nLaser Pistol\nat\nMCP Research\nis complete',True),('Not enough money to produce Laser Pistol',False),(None,False),('Production of Gauss Pistol is complete',False)]:
            with self.subTest(message=message):
                r=self.route();state={'state':'production_complete','visible':{'message':message}}
                r.read=lambda:state;r.ready=lambda _:state
                if passes:r.wait_completion('Laser Pistol')
                else:
                    with self.assertRaises(RouteError):r.wait_completion('Laser Pistol')
    def test_separate_phase_handoff_does_not_reset_research_count(self):
        r=ProductionCampaignRoute(lambda *a:None,lambda *a,**k:None,'xcom1',completion_allocation=True)
        r.count=73;r.incidents=lambda _:None;r.act=lambda *a,**kw:None
        with patch.object(ProductionRoute,'run_production') as run:
            r.complete_allocation('Laser Pistol');run.assert_called_once_with('Laser Pistol')
        self.assertEqual(r.count,73)

class Stores(unittest.TestCase):
    def route(self,pages):
        r=ProductionRoute(lambda *a:None,lambda *a,**k:None,'xcom1');index=[0];sent=[]
        r.ready=lambda _:pages[index[0]]
        def act(before,action,args,after):
            sent.append((action,args));index[0]=min(index[0]+1,len(pages)-1)
        r.act=act
        return r,sent
    def page(self,rows,down=True):
        return {'visible':{'rows':[{'cells':r} for r in rows],'scroll':{'down':down}}}
    def test_paged_known_item(self):
        r,sent=self.route([self.page([['Rifle','2','1']]),self.page([['Laser Pistol','3','0.9']]),self.page([['Ammo','2','1']],False)])
        self.assertEqual(r.scan_stores('Laser Pistol'),3);self.assertEqual(len(sent),2)
    def test_absence_only_after_boundary(self):
        page=self.page([['Rifle','2','1']]);r,sent=self.route([page,page])
        self.assertEqual(r.scan_stores('Laser Pistol'),0);self.assertEqual(len(sent),1)
    def test_unknown_name_blocks_absence(self):
        r,_=self.route([self.page([[None,'2','1']],False)])
        with self.assertRaisesRegex(RouteError,'unprovable'):r.scan_stores('Laser Pistol')
    def test_unknown_quantity_blocks_target(self):
        r,_=self.route([self.page([['Laser Pistol',None,'1']],False)])
        with self.assertRaises(RouteError):r.scan_stores('Laser Pistol')
    def test_duplicate_labels_fail(self):
        r,_=self.route([self.page([['Laser Pistol','2','1'],['Laser Pistol','3','1']],False)])
        with self.assertRaises(RouteError):r.scan_stores('Laser Pistol')
    def test_changed_target_quantity_fails(self):
        r,_=self.route([self.page([['Laser Pistol','2','1']]),self.page([['Laser Pistol','3','1']],False)])
        with self.assertRaisesRegex(RouteError,'changed'):r.scan_stores('Laser Pistol')
    def test_pagination_bound_stops(self):
        r,sent=self.route([self.page([[str(i),'1','1']]) for i in range(50)])
        with self.assertRaisesRegex(RouteError,'bound_reached'):r.scan_stores('Laser Pistol')
        self.assertEqual(len(sent),48)
    def test_empty_with_scroll_unknown_not_absence(self):
        r,_=self.route([self.page([])])
        with self.assertRaisesRegex(RouteError,'empty_scrolling'):r.scan_stores('Laser Pistol')

if __name__=='__main__':unittest.main()
