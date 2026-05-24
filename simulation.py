import pygame
import math
import sys

# Měřítko 1px = 3.33mm (SCALE = 0.3)
SCALE = 0.3
FIELD_W, FIELD_H = 1400, 2800
WIDTH, HEIGHT = int(FIELD_W * SCALE), int(FIELD_H * SCALE)
UI_WIDTH = 250

# Barvy
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
RED = (255, 50, 50)
GRAY = (200, 200, 200)
BLUE = (100, 100, 255)

FPS = 60

pygame.init()
screen = pygame.display.set_mode((WIDTH + UI_WIDTH, HEIGHT))
pygame.display.set_caption("Cheesecake - Perfektni Logika")
clock = pygame.time.Clock()

def check_wall_collision(x, y):
    if x < 10 or x > FIELD_W - 10 or y < 10 or y > FIELD_H - 10: return True
    # Zdi presne podle toho, aby tvorily S-pruchod
    if abs(x - 400) < 20 and 0 <= y <= 400: return True
    if abs(x - 900) < 20 and 300 <= y <= 1400: return True
    if abs(y - 1400) < 20 and 0 <= x <= 900: return True
    return False

class Robot:
    def __init__(self, x, y):
        self.start_x = x
        self.start_y = y
        self.x = x
        self.y = y
        self.angle = 0
        self.commands = []
        self.current_command = None
        self.is_moving = False
        self.trail = [(x, y)]

    def reset(self):
        self.x = self.start_x
        self.y = self.start_y
        self.angle = 0
        self.commands = []
        self.current_command = None
        self.is_moving = False
        self.trail = [(self.x, self.y)]

    def straight(self, dist):
        if dist > 0:
            self.commands.append(('straight', dist))

    def turn_right(self, deg):
        if deg > 0:
            self.commands.append(('turn', deg))

    def turn_left(self, deg):
        if deg > 0:
            self.commands.append(('turn', -deg))

    def update(self):
        if not self.is_moving and self.commands:
            self.current_command = self.commands.pop(0)
            self.is_moving = True

        if self.is_moving and self.current_command:
            cmd = self.current_command[0]
            
            if cmd == 'straight':
                val = self.current_command[1]
                speed = 10
                step = min(speed, abs(val)) * (1 if val > 0 else -1)
                self.x += step * math.sin(math.radians(self.angle))
                self.y += step * math.cos(math.radians(self.angle))
                self.trail.append((self.x, self.y))
                remaining = abs(val) - abs(step)
                if remaining <= 0:
                    self.is_moving = False
                else:
                    self.current_command = ('straight', remaining * (1 if val > 0 else -1))
            
            elif cmd == 'turn':
                val = self.current_command[1]
                speed = 4
                step = min(speed, abs(val)) * (1 if val > 0 else -1)
                self.angle += step
                remaining = val - step
                if abs(remaining) < 0.1:
                    self.is_moving = False
                else:
                    self.current_command = ('turn', remaining)

    def draw(self, surface):
        if len(self.trail) > 1:
            points = [(tx * SCALE, HEIGHT - ty * SCALE) for tx, ty in self.trail]
            pygame.draw.lines(surface, BLUE, False, points, 3)
            
        dx = self.x * SCALE
        dy = HEIGHT - self.y * SCALE
        pygame.draw.circle(surface, BLACK, (int(dx), int(dy)), int(150 * SCALE))
        end_x = dx + 30 * math.sin(math.radians(self.angle))
        end_y = dy - 30 * math.cos(math.radians(self.angle))
        pygame.draw.line(surface, WHITE, (dx, dy), (end_x, end_y), 3)

def draw_field():
    screen.fill(WHITE)
    
    # Srafovana zóna pro medveda - rozdelena na barevne sektory
    # Robot na tuto zonu kouka z pozice X=1150, Y=1450.
    # C++ kod bere X kladne smerem DOLEVA od robota.
    
    # Modry sektor (x < 400 z pohledu robota, coz znamena X > 750 absolutne)
    pygame.draw.rect(screen, (200, 220, 255), (750*SCALE, 0, (1400-750)*SCALE, (FIELD_H - 1400)*SCALE))
    # Cerveny sektor (400 <= x <= 600, coz je absolutne 550 az 750)
    pygame.draw.rect(screen, (255, 200, 200), (550*SCALE, 0, 200*SCALE, (FIELD_H - 1400)*SCALE))
    # Zluty sektor (x > 600 a y > 300, absolutne X < 550 a Y > 1750)
    # v Pygame je Y opacne, takze Y > 1750 znamena odshora do vysky (FIELD_H - 1750)
    pygame.draw.rect(screen, (255, 255, 200), (0, 0, 550*SCALE, (FIELD_H - 1750)*SCALE))
    # Zeleny sektor (x > 600 a y <= 300, absolutne X < 550 a Y = 1400 az 1750)
    pygame.draw.rect(screen, (200, 255, 200), (0, (FIELD_H - 1750)*SCALE, 550*SCALE, 350*SCALE))
    
    pygame.draw.rect(screen, BLACK, (0, 0, WIDTH, HEIGHT), 4)
    
    # 1. zed (leva)
    pygame.draw.line(screen, BLACK, (400*SCALE, HEIGHT), (400*SCALE, HEIGHT - 400*SCALE), 4)
    # 2. zed (stredni)
    pygame.draw.line(screen, BLACK, (900*SCALE, HEIGHT - 300*SCALE), (900*SCALE, HEIGHT - 1400*SCALE), 4)
    # 3. zed (horizontalni)
    pygame.draw.line(screen, BLACK, (0, HEIGHT - 1400*SCALE), (900*SCALE, HEIGHT - 1400*SCALE), 4)
    
    font = pygame.font.SysFont(None, 24)
    screen.blit(font.render("Start", True, BLACK), (100*SCALE, HEIGHT - 50*SCALE))
    
    # Popisky sektoru
    font_small = pygame.font.SysFont(None, 20)
    screen.blit(font_small.render("Zluty", True, (150, 150, 0)), (250*SCALE, 500*SCALE))
    screen.blit(font_small.render("Zeleny", True, (0, 150, 0)), (250*SCALE, 1100*SCALE))
    screen.blit(font_small.render("Cerveny", True, (150, 0, 0)), (600*SCALE, 800*SCALE))
    screen.blit(font_small.render("Modry", True, (0, 0, 150)), (1000*SCALE, 800*SCALE))

robot = Robot(200, 150)
state = 'WAITING_FOR_B'
bear_pos = None

running = True
while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        elif event.type == pygame.KEYDOWN:
            if event.key == pygame.K_b and state in ['WAITING_FOR_B', 'DONE']:
                mx, my = pygame.mouse.get_pos()
                if mx < WIDTH:
                    bx = mx / SCALE
                    by = (HEIGHT - my) / SCALE
                    # Lze kliknout jen do srafovane oblasti
                    if by >= 1400:
                        robot.reset()
                        bear_pos = (bx, by)
                        
                        # Prvni faze: GoToField
                        robot.straight(350)
                        robot.turn_right(90)
                        robot.straight(500)
                        robot.turn_right(90)
                        robot.straight(300)
                        robot.turn_left(90)
                        robot.straight(450)
                        robot.turn_left(90)
                        robot.straight(1250)
                        
                        state = 'AUTO_GOTOFIELD'
                        
            elif event.key == pygame.K_r:
                robot.reset()
                bear_pos = None
                state = 'WAITING_FOR_B'

    # Automaticky prechod mezi stavy (kdyz robot dojede predchozi sekvenci)
    if not robot.is_moving and len(robot.commands) == 0:
        if state == 'AUTO_GOTOFIELD':
            # Faze 2: GoForBear (Realisticka simulace podle main.cpp)
            bx, by = bear_pos
            dx = bx - robot.x
            dy = by - robot.y
            rad = math.radians(robot.angle)
            rel_x = dx * math.cos(rad) - dy * math.sin(rad)
            rel_y = dx * math.sin(rad) + dy * math.cos(rad)
            
            # V C++ kodu je X z kamery kladne DOLEVA
            cpp_x = -rel_x
            cpp_y = rel_y
            
            # V C++ kodu (Movement.hpp) funkce TurnRight posila M2(levy) dozadu a M3(pravy) dopredu -> toci DOLEVA
            # TurnLeft toci DOPRAVA. Simulujeme to tedy presne podle fyzickeho chovani robota:
            
            if cpp_x < 400:
                # Modry sektor
                robot.sector = 'blue'
                if cpp_y > 1400: cpp_y = 1300
                robot.straight(cpp_y)
                
            elif 400 <= cpp_x <= 600:
                # Cerveny sektor
                robot.sector = 'red'
                if cpp_y > 1400: cpp_y = 1300
                robot.straight(100)
                robot.turn_left(45) # C++ move.TurnRight(45)
                robot.straight(400)
                robot.turn_right(45) # C++ move.TurnLeft(45)
                robot.straight(cpp_y - 200)
                robot.turn_left(90) # C++ move.TurnRight(90) po sebrani
                
            elif cpp_x > 600 and cpp_y > 300:
                # Zluty sektor
                robot.sector = 'yellow'
                if cpp_y > 1400: cpp_y = 1300
                robot.straight(cpp_y)
                robot.turn_left(90) # C++ move.TurnRight(90)
                cx = cpp_x
                if cx > 1400: cx = 1300
                robot.straight(cx)
                
            elif cpp_x > 600 and cpp_y <= 300:
                # Zeleny sektor
                robot.sector = 'green'
                if cpp_y < 300: cpp_y = 300
                robot.straight(cpp_y)
                robot.turn_left(90) # C++ move.TurnRight(90)
                cx = cpp_x
                if cx > 1400: cx = 1300
                robot.straight(cx)
                
            state = 'AUTO_GOFORBEAR'
            
        elif state == 'AUTO_GOFORBEAR':
            # Faze 3: GoHome
            diff = (180 - robot.angle) % 360
            if diff > 180: diff -= 360
            if diff > 0: robot.turn_right(diff)
            elif diff < 0: robot.turn_left(-diff)
            
            if robot.y >= 1450:
                if robot.x < 1150:
                    robot.turn_left(90)
                    robot.straight(1150 - robot.x)
                    robot.turn_right(90)
                elif robot.x > 1150:
                    robot.turn_right(90)
                    robot.straight(robot.x - 1150)
                    robot.turn_left(90)
                    
                robot.straight(robot.y - 200)
                robot.turn_right(90)
                robot.straight(450)
                robot.turn_right(90)
                robot.straight(300)
                robot.turn_left(90)
                robot.straight(500)
                robot.turn_left(90)
                robot.straight(350)
                robot.turn_right(180)
                
            state = 'AUTO_GOHOME'
            
        elif state == 'AUTO_GOHOME':
            state = 'DONE'

    draw_field()
    
    if bear_pos:
        bx = bear_pos[0] * SCALE
        by = HEIGHT - bear_pos[1] * SCALE
        pygame.draw.circle(screen, (139, 69, 19), (int(bx), int(by)), 8)
        
    robot.update()
    robot.draw(screen)
    
    # UI
    font = pygame.font.SysFont(None, 24)
    ui_x = WIDTH + 10
    
    # Zobrazeni stavu
    display_state = state
    if state == 'AUTO_GOTOFIELD': display_state = "Jede do hriste..."
    elif state == 'AUTO_GOFORBEAR': display_state = "Chyta medveda..."
    elif state == 'AUTO_GOHOME': display_state = "Vraci se domu..."
    elif state == 'DONE': display_state = "Hotovo!"
    elif state == 'WAITING_FOR_B': display_state = "Ceka na zadani"
    
    screen.blit(font.render(f"Stav: {display_state}", True, BLACK), (ui_x, 20))
    screen.blit(font.render("Ovladani:", True, BLACK), (ui_x, 60))
    
    screen.blit(font.render("[B] Mys + B = Start", True, RED if state in ['WAITING_FOR_B', 'DONE'] else GRAY), (ui_x, 90))
    screen.blit(font.render("[R] Reset", True, BLACK), (ui_x, 120))

    if bear_pos:
        bx = bear_pos[0]
        by = bear_pos[1]
        screen.blit(font.render(f"Medved X: {int(bx)} mm", True, BLUE), (ui_x, 230))
        screen.blit(font.render(f"Medved Y: {int(by)} mm", True, BLUE), (ui_x, 260))
        
        # Hodnoty tak, jak je vidi robotova kamera z bodu X=1150, Y=1450 (X roste doleva)
        cpp_x = 1150 - bx
        cpp_y = by - 1450
        screen.blit(font.render(f"Kamera X: {int(cpp_x)} mm", True, (100,0,100)), (ui_x, 310))
        screen.blit(font.render(f"Kamera Y: {int(cpp_y)} mm", True, (100,0,100)), (ui_x, 340))

    pygame.display.flip()
    clock.tick(FPS)

pygame.quit()
sys.exit()
